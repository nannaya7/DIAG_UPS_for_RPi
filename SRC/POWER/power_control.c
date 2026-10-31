/**
  ******************************************************************************
  * @file    power_control.c
  * @brief   UPS power sequencing: MCU self-latch, boost enable, output gate,
  *          power button handling, and the SLEEP-mode wake-tick handler.
  *
  * Hardware notes (see cappic_ups_SCH.pdf):
  *  - LDO_OUT (PB11) is diode-ORed with the button and USB_PWR_IN into the
  *    MCU's own LDO enable pin. Driving it HIGH latches MCU power so the
  *    system stays alive after the user releases the button.
  *  - BOOST_EN (PA6) enables the battery boost converter (TPS61088). Per
  *    spec it must stay HIGH the whole time the board is "on" so DCDC is
  *    always ready if USB is unplugged.
  *  - VOUT_CTRL (PB1) is the only firmware-controlled step in the output
  *    path: it gates the (already source-selected, all-hardware) 5V rail
  *    out to the counterpart board. Active HIGH = output enabled.
  *  - USB_PLUG_IN (PB0) is status only; the USB-vs-battery source switch
  *    happens automatically in hardware from VBUS presence -- we can't see
  *    that switch directly, so we log USB_PLUG_IN transitions as the best
  *    available proxy for "source switched".
  *  - CHG_WKUP (PB12) is open-drain: idles floating (Hi-Z, released) and
  *    is driven LOW for ~500ms only on a power button press, to emulate a
  *    KEY press on the IP5310 power-bank IC without contending against
  *    the IC's own pull on that line.
  *
  * Call UpdatePowerControl() once after every SleepMode_Enter() wake, not
  * on a fixed cadence: button hold-time uses HAL_GetTick() timestamps (so
  * it doesn't need frequent polling), and the battery/DCDC sample + LED
  * flash only run when SleepMode_ConsumeWakeTick() says the 1s timer fired.
  *
  * Every state change gets a DebugLog_Printf() line (button press/release,
  * DCDC ON/OFF/FAULT, USB plug/unplug, output enable/disable, shutdown,
  * battery alert) -- except the LED flash itself, which stays silent.
  *
  * Main-board notifications (see at_protocol.c/.h): a short button press
  * sends AT+BTN0, a long press sends AT+BTN1, USB plug/unplug sends AT+USB0/
  * AT+USB1, AT+BATxx (SOC) and AT+BVTxx (cell voltage) go out together once
  * a minute (every BATT_REPORT_PERIOD_TICKS-th 1s tick), and a MAX17048
  * ALRT interrupt pushes AT+BATL immediately. All of those are
  * fire-and-forget. A 5s button hold is a forced-off path: it sends AT+PWR0
  * but never waits for a response before VOUT_CTRL/DCDC are cut.
  * (A periodic AT+PING heartbeat was tried and removed 2026-07-30 -- decided
  * unnecessary.)
  *
  * Button-press visual confirmation (2026-09-08): independent of whatever
  * breathing/blink effect is currently selected, a short press blinks WHITE
  * once and a 5s long-press hold blinks WHITE twice (PowerControl_BlinkWhite())
  * right where AT+BTN0/AT+BTN1 are sent. Once a long press is confirmed, the
  * LED switches to a one-shot GREEN+RED fade from full brightness to off
  * over SHUTDOWN_FADE_DURATION_MS, then power is cut unconditionally.
  *
  * Boot-cause gated power-on (2026-09-08): U3(LDO)'s VEN is diode-ORed from
  * the button (PB_SW) AND from USB_PWR_IN (schematic net, same node as
  * USB_PLUG_IN/PB0), so plugging in USB alone can boot the MCU without the
  * button ever being pressed -- by design, so charge status can be checked
  * while the board is otherwise "off". PowerControl_Init() now checks
  * BTN_PWR_IN first: if it's not actually pressed, it blocks in
  * PowerControl_ChargeCheckStandby() (RED/BLUE breathing LED from the
  * MAX17048 SOC, BOOST_EN/VOUT_CTRL left off, no self-latch) until a real
  * button press is seen. LED while charging: RED breathing; LED once the
  * SOC read hits 100% ("charge complete"): off entirely.
  *
  * 2026-10: boot-time standby removed -- plugging in USB alone now powers on
  * immediately (PowerControl_Init() goes straight to the power-on sequence).
  * The standby is still used after a shutdown while USB stays plugged, so
  * the board can actually be turned off.
  *
  * Battery-only low-voltage lockout (2026-09-10): a real button press
  * still self-latches immediately (see PowerControl_PowerOnSequence()),
  * but if USB is absent and the cell reads at or below LOWBATT_REJECT_MV,
  * BOOST_EN/VOUT_CTRL are never turned on -- instead the LED blinks RED
  * a few times (PowerControl_LowBatteryReject()) and the self-latch is
  * released again, refusing to power the output stage from a battery too
  * weak to sustain it without USB assist.
  *
  * Power-ON LED (2026-09-08, USB-based default added same day): once fully
  * on, the LED defaults to GREEN breathing while USB is plugged in, or BLUE
  * breathing while running on battery alone -- see
  * PowerControl_ApplyDefaultLed(), live-updated on every USB plug/unplug
  * via PowerControl_CheckUsbPlugChange(). Sending AT+LED,RR,GG,BB,PP (see
  * at_protocol.c) overrides it with a custom blink and, via
  * PowerControl_NotifyLedOverridden(), stops further USB-driven color
  * changes until the next power-on -- reset back to the USB-based default
  * every time PowerControl_PowerOnSequence() runs, so the main board must
  * resend AT+LED after every power-on.
  *
  * Internal status flags (no protocol/wire changes -- just state tracking):
  *  - MCU_STATUS: mcu_power.c/.h (POWERING_ON/POWER_ON/POWERING_OFF/
  *    POWER_OFF), set at the start/end of PowerControl_Init()/_Shutdown().
  *  - BATT_STATUS: batt_status.c/.h (NORMAL/LOW), decided purely from the
  *    SOC percentage on each once-a-minute report (<=BATT_STATUS_LOW_PCT ->
  *    LOW, else NORMAL) -- deliberately NOT tied to the MAX17048 ALRT
  *    interrupt, since that pin can spuriously fire independent of SOC.
  *  - DCDC_POWER_GOOD: Dcdc_IsPowerGood() in dcdc.c/.h, true iff DCDC state
  *    is ON (not OFF/FAULT) -- a direct view of the state dcdc.c already
  *    tracks, not a separately-stored flag.
  *  - USB_STATUS: PowerControl_GetUsbStatus() below, IN/OUT mirroring the
  *    existing s_usbPlugged tracked by PowerControl_CheckUsbPlugChange().
  ******************************************************************************
  */
#include "power_control.h"
#include "max17048.h"
#include "debug_log.h"
#include "dcdc.h"
#include "sleep_mode.h"
#include "rgb_led.h"
#include "at_protocol.h"
#include "mcu_power.h"
#include "batt_status.h"

#define BTN_LONG_PRESS_MS      5000U   /* hold this long while ON -> shutdown */
#define BTN_RELEASE_DEBOUNCE_MS 30U   /* stable release before re-arming after shutdown */
#define POWERON_SETTLE_MS      700U    /* let BOOST_EN/DCDC settle before VOUT*/
#define CHG_WKUP_PULSE_MS      500U
#define SHUTDOWN_LED_PEAK       255U   /* GREEN+RED fade-out starting brightness */
#define SHUTDOWN_FADE_DURATION_MS 5000U
#define BATT_REPORT_PERIOD_TICKS 60U   /* AT+BATxx cadence: once per minute (1Hz ticks) */
#define BATT_STATUS_LOW_PCT      15U   /* SOC at or below this -> BATT_STATUS_LOW */
#define SOC_ZERO_RETRY_INTERVAL_MS 10000U
#define SOC_ZERO_RETRY_COUNT        3U

#define LED_BREATHE_HALF_PERIOD_MS 3000U /* 3s brighten + 3s dim = 6s cycle */
#define LED_BREATHE_PEAK           255U
#define CHARGE_SOC_COMPLETE_PCT    100U

#define BTN_BLINK_WHITE_PEAK    128U   /* ~50% brightness -- full 255 was too bright */
#define BTN_BLINK_ON_MS         150U
#define BTN_BLINK_OFF_MS        150U

/* Battery-only (no USB) power-on is refused at or below this cell voltage
   -- too low to trust the boost/output stage without USB assist. */
#define LOWBATT_REJECT_MV       3500U
#define LOWBATT_BLINK_RED_PEAK  255U
#define LOWBATT_BLINK_ON_MS     500U
#define LOWBATT_BLINK_OFF_MS    500U
#define LOWBATT_BLINK_COUNT     3U

static uint8_t  s_btnHeld         = 0U;
static uint32_t s_btnPressStart   = 0U;
static uint8_t  s_shutdownLatched = 0U;
static volatile uint8_t s_powerOffRequested = 0U; /* UART ISR -> main loop */

/* Set when the button causing this power-on is still physically held at
   the moment PowerControl_PowerOnSequence() runs -- suppresses the AT+BTN0
   report/white blink for that one release, so only presses that start
   after the board is already on get reported as a short press. */
static uint8_t  s_suppressNextBtnReport = 0U;

static uint8_t  s_usbPlugged      = 0U;
static uint8_t  s_usbInited       = 0U;

/* Tracks whether the power-on default LED (GREEN on USB / BLUE on battery,
   see PowerControl_ApplyDefaultLed()) is still showing, so a USB plug/
   unplug event doesn't stomp on a color the main board explicitly set via
   AT+LED. Cleared back to "default" at the start of every new ON session. */
static uint8_t  s_ledOverridden   = 0U;

static volatile uint8_t s_battAlertPending = 0U;
static uint32_t s_battReportTickCount = 0U;
static uint8_t  s_socZeroVerifyActive = 0U;
static uint8_t  s_socZeroRetryCount = 0U;
static uint32_t s_socZeroLastRetryTick = 0U;

/* Clears and reports a pending MAX17048 ALRT interrupt, if any. Shared by
   UpdatePowerControl() and PowerControl_ChargeCheckStandby() so a
   low-battery alert during USB-only charge-check standby is still
   serviced (cleared on the chip, logged locally) instead of sitting
   latched, unreported, until whenever the board eventually reaches full
   power-on. AtProtocol_SendBatteryLow() is fire-and-forget either way --
   harmless to call even while the main board has no power to receive it. */
static void PowerControl_ServiceBatteryAlert(void)
{
  if (s_battAlertPending != 0U)
  {
    s_battAlertPending = 0U;
    Max17048_ClearAlert();
    DebugLog_Printf("battery: low-SOC alert\r\n");
    AtProtocol_SendBatteryLow();
  }
}

static void PowerControl_PublishBatteryReport(uint8_t socPct)
{
  AtProtocol_SendBattery(socPct);
  AtProtocol_SendBatteryVoltage(Max17048_ReadVoltageMv());
  BattStatus_Set((socPct <= BATT_STATUS_LOW_PCT) ? BATT_STATUS_LOW : BATT_STATUS_NORMAL);
}

/* MAX17048 I2C failure and a real 0% SOC both currently read as zero.
   Before exposing that value to Radxa, require three more zero readings at
   10-second intervals. A nonzero retry is accepted and reported at once. */
static void PowerControl_StartBatteryReport(void)
{
  uint8_t socPct = (uint8_t)Max17048_ReadSocPercent();

  if (socPct != 0U)
  {
    PowerControl_PublishBatteryReport(socPct);
    return;
  }

  s_socZeroVerifyActive = 1U;
  s_socZeroRetryCount = 0U;
  s_socZeroLastRetryTick = HAL_GetTick();
  DebugLog_Printf("battery: SOC changed/read as 0%% -> verify 3 times\r\n");
}

static void PowerControl_ServiceSocZeroVerification(void)
{
  uint8_t socPct;

  if ((s_socZeroVerifyActive == 0U) ||
      ((HAL_GetTick() - s_socZeroLastRetryTick) < SOC_ZERO_RETRY_INTERVAL_MS))
  {
    return;
  }

  s_socZeroLastRetryTick = HAL_GetTick();
  s_socZeroRetryCount++;
  socPct = (uint8_t)Max17048_ReadSocPercent();

  DebugLog_Printf("battery: SOC 0%% verify %u/%u -> %u%%\r\n",
                   (unsigned int)s_socZeroRetryCount,
                   (unsigned int)SOC_ZERO_RETRY_COUNT,
                   (unsigned int)socPct);

  if (socPct != 0U)
  {
    s_socZeroVerifyActive = 0U;
    PowerControl_PublishBatteryReport(socPct);
  }
  else if (s_socZeroRetryCount >= SOC_ZERO_RETRY_COUNT)
  {
    s_socZeroVerifyActive = 0U;
    PowerControl_PublishBatteryReport(0U);
  }
}

/* Blocks here (BOOST_EN/VOUT_CTRL/LDO_OUT untouched, so DCDC and the
   output stage stay off and nothing is self-latched) until BTN_PWR_IN is
   actually pressed, showing charge status from the MAX17048 SOC in the
   meantime: RED breathing while charging, LED off once SOC reads 100%
   ("charge complete"). If USB is unplugged before a press happens, VEN
   drops and the MCU just dies here -- expected, no special handling needed.
   Called both from PowerControl_Init() (USB-only boot, no button pressed
   yet) and after PowerControl_CutPower() (board was just turned off but
   USB is still holding VEN up, so the MCU is still alive and should go
   back to showing charge status instead of sitting idle and dark). */
static void PowerControl_ChargeCheckStandby(uint8_t waitForRelease)
{
  uint8_t lastComplete = 0xFFU; /* neither 0 nor 1 -> forces the first LED pick */
  uint32_t releaseStart = HAL_GetTick();

  DebugLog_Printf("power: charge-check standby\r\n");

  for (;;)
  {
    uint8_t pressed = (HAL_GPIO_ReadPin(BTN_PWR_IN_GPIO_Port, BTN_PWR_IN_Pin) == GPIO_PIN_SET);

    /* A held shutdown button must never count as a new power-on press.
       Keep servicing charge status while waiting for a stable release;
       cold boot skips this guard so its initial press still powers on. */
    if (waitForRelease != 0U)
    {
      if (pressed != 0U)
      {
        releaseStart = HAL_GetTick();
      }
      else if ((HAL_GetTick() - releaseStart) >= BTN_RELEASE_DEBOUNCE_MS)
      {
        waitForRelease = 0U;
      }
    }
    else if (pressed != 0U)
    {
      DebugLog_Printf("power: button press -> full power-on\r\n");
      return;
    }

    PowerControl_ServiceBatteryAlert();

    if ((lastComplete == 0xFFU) || (SleepMode_ConsumeWakeTick() != 0U))
    {
      uint8_t complete = (Max17048_ReadSocPercent() >= CHARGE_SOC_COMPLETE_PCT) ? 1U : 0U;

      if (complete != lastComplete)
      {
        if (complete != 0U)
        {
          /* Charge complete: LED off entirely (was BLUE breathing).
             halfPeriodMs=0 -> RgbLed_Update()'s "solid on" branch, so this
             stays off on every subsequent call instead of being fought
             over by the effect engine's own animation. */
          RgbLed_SetEffect(RGBLED_EFFECT_BREATHE, 0U, 0U, 0U, 0U);
        }
        else
        {
          RgbLed_SetEffect(RGBLED_EFFECT_BREATHE, LED_BREATHE_PEAK, 0U, 0U, LED_BREATHE_HALF_PERIOD_MS);
        }
        lastComplete = complete;
      }
    }

    RgbLed_Update();
  }
}

/* Power-on default LED: GREEN breathing while USB is supplying power, BLUE
   breathing while running on battery alone (USB unplugged) -- gives a
   passive at-a-glance power-source indication even before/without the main
   board sending AT+LED. Stays in effect until either an AT+LED command
   overrides it (see PowerControl_NotifyLedOverridden()) or the next
   power-on session resets the override flag. */
static void PowerControl_ApplyDefaultLed(uint8_t usbPlugged)
{
  if (usbPlugged != 0U)
  {
    RgbLed_SetEffect(RGBLED_EFFECT_BREATHE, 0U, LED_BREATHE_PEAK, 0U, LED_BREATHE_HALF_PERIOD_MS);
  }
  else
  {
    RgbLed_SetEffect(RGBLED_EFFECT_BREATHE, 0U, 0U, LED_BREATHE_PEAK, LED_BREATHE_HALF_PERIOD_MS);
  }
}

/* Battery-only (no USB) power-on refused because the cell is too low to
   trust the boost/output stage without USB assist: blinks RED
   LOWBATT_BLINK_COUNT times instead of turning on BOOST_EN/VOUT_CTRL, then
   releases the self-latch and waits for the triggering press to be
   released before returning. The wait matters -- without it, a button
   still held down would immediately re-trigger this same rejection in a
   tight loop the instant PowerControl_ChargeCheckStandby() saw it high
   again. */
static void PowerControl_LowBatteryReject(void)
{
  uint8_t i;

  DebugLog_Printf("power: battery too low (%umV) for battery-only power-on -> reject\r\n",
                   Max17048_ReadVoltageMv());

  for (i = 0U; i < LOWBATT_BLINK_COUNT; i++)
  {
    RgbLed_SetColor(LOWBATT_BLINK_RED_PEAK, 0U, 0U);
    HAL_Delay(LOWBATT_BLINK_ON_MS);
    RgbLed_Off();
    HAL_Delay(LOWBATT_BLINK_OFF_MS);
  }

  McuPower_SetState(MCU_POWER_OFF);
  HAL_GPIO_WritePin(LDO_OUT_GPIO_Port, LDO_OUT_Pin, GPIO_PIN_RESET);

  /* USB is absent (that's why we're here), so only the still-held button
     (via D3) can be keeping VEN -- and therefore the MCU -- alive past the
     self-latch release. Wait for it to be released; VEN then drops and
     the MCU dies right here, same "expected, not handled specially" idiom
     as the normal power-cut path. */
  while (HAL_GPIO_ReadPin(BTN_PWR_IN_GPIO_Port, BTN_PWR_IN_Pin) == GPIO_PIN_SET)
  {
    /* spin */
  }
}

/* Self-latches, brings up DCDC/VOUT, and sets the default power-on LED --
   the "fully power on" tail shared by PowerControl_Init() (at boot) and
   PowerControl_CutPower() (when resuming after a shutdown that didn't
   actually kill the MCU, see below). This is the only place in the
   codebase that drives VOUT_CTRL high, so Q7 and the default LED can never
   come back out of sync with each other. */
static void PowerControl_PowerOnSequence(void)
{
  uint8_t usbPlugged;

  /* Latch our own power first so releasing the button doesn't kill us. */
  HAL_GPIO_WritePin(LDO_OUT_GPIO_Port, LDO_OUT_Pin, GPIO_PIN_SET);

  usbPlugged = (HAL_GPIO_ReadPin(USB_PLUG_IN_GPIO_Port, USB_PLUG_IN_Pin) == GPIO_PIN_SET) ? 1U : 0U;

  /* Refuse to power the boost/output stage from a critically low battery
     with no USB to help -- see PowerControl_LowBatteryReject(). USB
     presence alone is enough to skip this check: DCDC/VOUT draw from
     whichever source the hardware has already OR'd onto the 5V rail, not
     straight off the cell, so a low cell with USB plugged in is fine. */
  if ((usbPlugged == 0U) && (Max17048_ReadVoltageMv() <= LOWBATT_REJECT_MV))
  {
    PowerControl_LowBatteryReject();
    return;
  }

  /* Boost stays on for the whole "board on" session, regardless of USB. */
  Dcdc_On();
  HAL_Delay(POWERON_SETTLE_MS);

  /* Enable the output to the counterpart board. */
  HAL_GPIO_WritePin(VOUT_CTRL_GPIO_Port, VOUT_CTRL_Pin, GPIO_PIN_SET);
  DebugLog_Printf("vout: enabled\r\n");

  /* New ON session -- any previous AT+LED override no longer applies. */
  s_ledOverridden = 0U;
  s_battReportTickCount = 0U;
  s_socZeroVerifyActive = 0U;
  s_socZeroRetryCount = 0U;
  PowerControl_ApplyDefaultLed(usbPlugged);

  /* If the button that caused this power-on (cold boot with it held, or
     the press that woke PowerControl_ChargeCheckStandby()) is still
     physically down, prime the button state machine so
     PowerControl_HandleButton() doesn't treat that same press as a
     brand-new one once it's released -- only a press that starts after
     the board is already on should count as a reportable short/long
     press. Restarting s_btnPressStart from here also means the 5s
     long-press timer counts from power-on, not from whenever the button
     first went down pre-boot. */
  if (HAL_GPIO_ReadPin(BTN_PWR_IN_GPIO_Port, BTN_PWR_IN_Pin) == GPIO_PIN_SET)
  {
    s_btnHeld               = 1U;
    s_btnPressStart         = HAL_GetTick();
    s_suppressNextBtnReport = 1U;
  }

  McuPower_SetState(MCU_POWER_ON);
}

void PowerControl_Init(void)
{
  /* USB-only boot powers on directly: no charge-check standby here. */
  PowerControl_PowerOnSequence();
}

/* Quick blocking white blink(s), independent of whatever breathe/blink
   effect is currently selected -- gives the user immediate visual
   confirmation that a short press (1 blink) or a 5s long-press hold
   (2 blinks) was actually registered. Bypasses the effect engine via
   RgbLed_SetColor()/RgbLed_Off() directly; the running effect's own state
   (RgbLed_SetEffect()) is untouched, so it resumes on its own from the
   next RgbLed_Update() call once this returns. */
static void PowerControl_BlinkWhite(uint8_t times)
{
  uint8_t i;

  for (i = 0U; i < times; i++)
  {
    RgbLed_SetColor(BTN_BLINK_WHITE_PEAK, BTN_BLINK_WHITE_PEAK, BTN_BLINK_WHITE_PEAK);
    HAL_Delay(BTN_BLINK_ON_MS);
    RgbLed_Off();
    HAL_Delay(BTN_BLINK_OFF_MS);
  }
}

void PowerControl_PulseChgWakeup(void)
{
  DebugLog_Printf("chg_wkup: pulse\r\n");
  HAL_GPIO_WritePin(CHG_WKUP_GPIO_Port, CHG_WKUP_Pin, GPIO_PIN_RESET);
  HAL_Delay(CHG_WKUP_PULSE_MS);
  HAL_GPIO_WritePin(CHG_WKUP_GPIO_Port, CHG_WKUP_Pin, GPIO_PIN_SET);
}

static void PowerControl_LogStatus(void)
{
  uint16_t socPct = Max17048_ReadSocPercent();
  uint16_t cellMv = Max17048_ReadVoltageMv();

  DebugLog_Printf("dcdc=%s soc=%u%% cell=%umV usb=%u\r\n",
                   Dcdc_StateName(Dcdc_GetState()),
                   socPct, cellMv, (unsigned int)s_usbPlugged);
  DebugLog_Printf("MCU_STATUS=%s BATT_STATUS=%s DCDC_POWER_GOOD=%u USB_STATUS=%s\r\n",
                   McuPower_StateName(McuPower_GetState()),
                   BattStatus_Name(BattStatus_Get()),
                   (unsigned int)Dcdc_IsPowerGood(),
                   PowerControl_UsbStatusName(PowerControl_GetUsbStatus()));
}

static void PowerControl_RunShutdownFade(uint32_t durationMs)
{
  uint32_t start;

  RgbLed_SetEffect(RGBLED_EFFECT_FADE_OUT, SHUTDOWN_LED_PEAK,
                   SHUTDOWN_LED_PEAK, 0U, durationMs);
  start = HAL_GetTick();
  while ((HAL_GetTick() - start) < durationMs)
  {
    RgbLed_Update();
    HAL_Delay(5U);
  }
  RgbLed_Off();
}

static void PowerControl_CutPower(void)
{
  HAL_GPIO_WritePin(VOUT_CTRL_GPIO_Port, VOUT_CTRL_Pin, GPIO_PIN_RESET);
  DebugLog_Printf("vout: disabled\r\n");
  Dcdc_Off();
  RgbLed_Off();
  McuPower_SetState(MCU_POWER_OFF);
  HAL_GPIO_WritePin(LDO_OUT_GPIO_Port, LDO_OUT_Pin, GPIO_PIN_RESET);

  s_btnHeld = 0U;
  s_shutdownLatched = 0U;
  s_suppressNextBtnReport = 0U;

  PowerControl_ChargeCheckStandby(1U);
  PowerControl_PowerOnSequence();
}

/* UART receive runs in interrupt context: defer all blocking work to the
   main loop. Ignore requests outside ON, including shutdown and standby. */
void PowerControl_RequestPowerOff(void)
{
  if (McuPower_GetState() == MCU_POWER_ON)
  {
    s_powerOffRequested = 1U;
  }
}

/* A 5s button hold keeps the original MCU-initiated shutdown handshake. */
static void PowerControl_BeginButtonShutdown(void)
{
  McuPower_SetState(MCU_POWERING_OFF);
  s_powerOffRequested = 0U;
  s_shutdownLatched = 1U;
  AtProtocol_SendBtnLongPress();
  PowerControl_BlinkWhite(2U);
  AtProtocol_SendPowerOffRequest();
  PowerControl_RunShutdownFade(SHUTDOWN_FADE_DURATION_MS);
  PowerControl_CutPower();
}

static void PowerControl_BeginRemoteShutdown(void)
{
  DebugLog_Printf("power: Radxa AT+PWR0 -> shutdown\r\n");
  McuPower_SetState(MCU_POWERING_OFF);
  s_powerOffRequested = 0U;
  s_shutdownLatched = 1U;

  AtProtocol_SendPowerOffAck();
  PowerControl_BlinkWhite(2U);
  PowerControl_RunShutdownFade(SHUTDOWN_FADE_DURATION_MS);
  PowerControl_CutPower();
}

/* PA0 is active-high (pressed = HIGH, see SW3/R38/R33 divider in the
   schematic) and already wakes SLEEP mode by itself via EXTI0 -- this just
   times how long it's been held using wall-clock timestamps, so it works
   fine even though we're not polling every 10ms anymore. */
static void PowerControl_HandleButton(void)
{
  uint8_t pressed = (HAL_GPIO_ReadPin(BTN_PWR_IN_GPIO_Port, BTN_PWR_IN_Pin) == GPIO_PIN_SET);

  if (pressed)
  {
    if (s_btnHeld == 0U)
    {
      s_btnHeld         = 1U;
      s_btnPressStart   = HAL_GetTick();
      s_shutdownLatched = 0U;
      DebugLog_Printf("button: pressed\r\n");
      PowerControl_LogStatus();
      PowerControl_PulseChgWakeup();
    }
    else if ((s_shutdownLatched == 0U) &&
             ((HAL_GetTick() - s_btnPressStart) >= BTN_LONG_PRESS_MS))
    {
      PowerControl_BeginButtonShutdown();
    }
  }
  else
  {
    if (s_btnHeld != 0U)
    {
      DebugLog_Printf("button: released\r\n");
      if ((s_shutdownLatched == 0U) && (s_suppressNextBtnReport == 0U))
      {
        AtProtocol_SendBtnShortPress();
        PowerControl_BlinkWhite(1U); /* confirms the short press was recognized */
      }
      s_suppressNextBtnReport = 0U; /* only ever suppresses the one press */
    }
    s_btnHeld = 0U;
  }
}

static void PowerControl_CheckUsbPlugChange(void)
{
  uint8_t plugged = (HAL_GPIO_ReadPin(USB_PLUG_IN_GPIO_Port, USB_PLUG_IN_Pin) == GPIO_PIN_SET);

  if ((s_usbInited == 0U) || (plugged != s_usbPlugged))
  {
    DebugLog_Printf("usb: %s\r\n", plugged ? "plugged" : "unplugged");
    if (plugged)
    {
      AtProtocol_SendUsbPlugIn();
    }
    else
    {
      AtProtocol_SendUsbPlugOut();
    }
    s_usbPlugged = plugged;
    s_usbInited  = 1U;

    /* Live GREEN<->BLUE switch on USB plug/unplug, but only while still
       showing the default LED -- an AT+LED override from the main board
       takes priority until the next power-on. */
    if (s_ledOverridden == 0U)
    {
      PowerControl_ApplyDefaultLed(plugged);
    }
  }
}

void PowerControl_NotifyLedOverridden(void)
{
  s_ledOverridden = 1U;
}

void UpdatePowerControl(void)
{
  if (s_powerOffRequested != 0U)
  {
    PowerControl_BeginRemoteShutdown();
    return;
  }

  PowerControl_HandleButton();

  PowerControl_ServiceBatteryAlert();
  PowerControl_ServiceSocZeroVerification();

  if (SleepMode_ConsumeWakeTick() != 0U)
  {
    PowerControl_CheckUsbPlugChange();
    Dcdc_Update();
    s_battReportTickCount++;
    if (s_battReportTickCount >= BATT_REPORT_PERIOD_TICKS)
    {
      s_battReportTickCount = 0U;
      if (s_socZeroVerifyActive == 0U)
      {
        PowerControl_StartBatteryReport();
      }
    }
  }

  /* Not tick-gated: called every loop pass (the main loop no longer
     sleeps) so the breathing/blinking LED effect looks smooth instead of
     stepping once a second. Suppressed once shutdown is latched so it
     doesn't overwrite the shutdown fade. */
  if (s_shutdownLatched == 0U)
  {
    RgbLed_Update();
  }
}

UsbStatus_t PowerControl_GetUsbStatus(void)
{
  return (s_usbPlugged != 0U) ? USB_STATUS_IN : USB_STATUS_OUT;
}

const char *PowerControl_UsbStatusName(UsbStatus_t status)
{
  return (status == USB_STATUS_IN) ? "IN" : "OUT";
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == BATT_ALT_Pin)
  {
    s_battAlertPending = 1U;
  }
  /* BTN_PWR_IN_Pin: no action needed here -- EXTI0 wakes SLEEP mode by
     itself, and UpdatePowerControl() reads the pin level after every
     wake to time the hold duration. */
}
