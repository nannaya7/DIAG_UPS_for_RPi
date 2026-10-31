/**
  ******************************************************************************
  * @file    power_control.c
  * @brief   UPS power sequencing: MCU self-latch, boost enable, output gate,
  *          power button handling, USB plug tracking.
  *
  * Hardware notes (see DIAG_UPS1.2_SCH.pdf):
  *  - LDO_OUT (PB13) is diode-ORed with the button and USB_PWR_IN into the
  *    MCU's own LDO enable pin. Driving it HIGH latches MCU power so the
  *    system stays alive after the user releases the button.
  *  - BOOST_EN (PA15) enables the battery boost converter (TPS61088). It
  *    stays HIGH the whole time the board is "on" so DCDC is always ready
  *    if USB is unplugged.
  *  - VOUT_CTRL (PB14) is the only firmware-controlled step in the output
  *    path: it gates the (already source-selected, all-hardware) 5V rail
  *    out to the counterpart board. Active HIGH = output enabled.
  *  - USB_PWR_IN (PA6) is USB presence (HIGH = USB plugged); it is status only; the USB-vs-battery source switch
  *    happens automatically in hardware from VBUS presence.
  *  - CHG_WKUP (PB12) is open-drain: idles floating (released) and is
  *    driven LOW for ~500ms on a power button press to emulate a KEY press
  *    on the IP5310 power-bank IC.
  *  - BTN_PWR_IN (PA0) is active HIGH.
  *
  * Power-on:
  *  - Boot (button or USB) goes straight to PowerControl_PowerOnSequence().
  *  - Battery-only boot with the cell at or below LOWBATT_REJECT_MV is
  *    refused: RED blinks, the self-latch is released again.
  *  - After a shutdown with USB still plugged (MCU kept alive by VEN), the
  *    board waits in charge-check standby for a new button press.
  *
  * Shutdown: a 5s button hold (or a queued AT+PWR0 request) runs the
  * shutdown sequence -- white blinks, a fade-out, then power is cut.
  *
  * Notifications to the main board go through at_protocol.c (currently
  * disabled, see AT_PROTOCOL_ENABLED). Every state change is logged with
  * DebugLog_Printf().
  *
  * Status flags (state tracking only):
  *  - MCU_STATUS: mcu_power.c, BATT_STATUS: batt_status.c,
  *    DCDC_POWER_GOOD: Dcdc_IsPowerGood(), USB_STATUS: PowerControl_GetUsbStatus().
  ******************************************************************************
  */
#include "power_control.h"
#include "max17048.h"
#include "batt_report.h"
#include "debug_log.h"
#include "dcdc.h"
#include "sleep_mode.h"
#include "rgb_led.h"
#include "led_indicator.h"
#include "at_protocol.h"
#include "mcu_power.h"
#include "batt_status.h"

#define BTN_LONG_PRESS_MS         5000U  /* hold this long while ON -> shutdown */
#define BTN_RELEASE_DEBOUNCE_MS   30U    /* stable release before re-arming after shutdown */
#define POWERON_SETTLE_MS         700U   /* let BOOST_EN/DCDC settle before VOUT */
#define CHG_WKUP_PULSE_MS         500U
#define SHUTDOWN_FADE_DURATION_MS 5000U
#define CHARGE_SOC_COMPLETE_PCT   100U

/* Battery-only (no USB) power-on is refused at or below this cell voltage
   -- too low to trust the boost/output stage without USB assist. */
#define LOWBATT_REJECT_MV         3500U

static uint8_t  s_btnHeld         = 0U;
static uint32_t s_btnPressStart   = 0U;
static uint8_t  s_shutdownLatched = 0U;
static volatile uint8_t s_powerOffRequested = 0U; /* UART ISR -> main loop */

/* Set when the button causing this power-on is still physically held at the
   moment PowerControl_PowerOnSequence() runs -- suppresses the short-press
   report/blink for that one release. */
static uint8_t  s_suppressNextBtnReport = 0U;

static uint8_t  s_usbPlugged = 0U;
static uint8_t  s_usbInited  = 0U;

/* True once an AT+LED command overrode the power-source LED, so USB plug
   events stop changing the color. Reset at the start of every ON session. */
static uint8_t  s_ledOverridden = 0U;

static uint8_t PowerControl_IsButtonPressed(void)
{
  return (HAL_GPIO_ReadPin(BTN_PWR_IN_GPIO_Port, BTN_PWR_IN_Pin) == GPIO_PIN_SET) ? 1U : 0U;
}

static uint8_t PowerControl_IsUsbPlugged(void)
{
  return (HAL_GPIO_ReadPin(USB_PWR_IN_GPIO_Port, USB_PWR_IN_Pin) == GPIO_PIN_SET) ? 1U : 0U;
}

/* Blocks (BOOST_EN/VOUT_CTRL/LDO_OUT untouched, so DCDC and the output
   stage stay off) until BTN_PWR_IN is pressed, showing charge status from
   the MAX17048 SOC meanwhile. If USB is unplugged before a press, VEN drops
   and the MCU simply dies here. Used after PowerControl_CutPower(), where
   USB still holds VEN up.
   waitForRelease: ignore the button until it has been released for
   BTN_RELEASE_DEBOUNCE_MS, so the shutdown press doesn't count as a new
   power-on press. */
static void PowerControl_ChargeCheckStandby(uint8_t waitForRelease)
{
  uint8_t lastComplete = 0xFFU; /* neither 0 nor 1 -> forces the first LED pick */
  uint32_t releaseStart = HAL_GetTick();

  DebugLog_Printf("power: charge-check standby\r\n");

  for (;;)
  {
    uint8_t pressed = PowerControl_IsButtonPressed();

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

    BattReport_ServiceAlert();

    if ((lastComplete == 0xFFU) || (SleepMode_ConsumeWakeTick() != 0U))
    {
      uint8_t complete = (Max17048_ReadSocPercent() >= CHARGE_SOC_COMPLETE_PCT) ? 1U : 0U;

      if (complete != lastComplete)
      {
        LedIndicator_ShowChargeStatus(complete);
        lastComplete = complete;
      }
    }

    RgbLed_Update();
  }
}

/* Battery-only power-on refused: blinks RED, releases the self-latch, then
   waits for the triggering press to be released (otherwise a held button
   would re-trigger the rejection in a tight loop). With USB absent, only
   the held button can keep VEN alive, so the MCU dies here once released. */
static void PowerControl_LowBatteryReject(void)
{
  DebugLog_Printf("power: battery too low (%umV) for battery-only power-on -> reject\r\n",
                   Max17048_ReadVoltageMv());

  LedIndicator_BlinkLowBattery();

  McuPower_SetState(MCU_POWER_OFF);
  HAL_GPIO_WritePin(LDO_OUT_GPIO_Port, LDO_OUT_Pin, GPIO_PIN_RESET);

  while (PowerControl_IsButtonPressed() != 0U)
  {
    /* spin */
  }
}

/* Self-latches, brings up DCDC/VOUT and sets the default LED. Shared by
   PowerControl_Init() and PowerControl_CutPower(). The only place that
   drives VOUT_CTRL high. */
static void PowerControl_PowerOnSequence(void)
{
  uint8_t usbPlugged;

  /* Latch our own power first so releasing the button doesn't kill us. */
  HAL_GPIO_WritePin(LDO_OUT_GPIO_Port, LDO_OUT_Pin, GPIO_PIN_SET);

  usbPlugged = PowerControl_IsUsbPlugged();

  /* USB presence alone skips the low-battery check: DCDC/VOUT draw from
     whichever source the hardware already OR'd onto the 5V rail. */
  if ((usbPlugged == 0U) && (Max17048_ReadVoltageMv() <= LOWBATT_REJECT_MV))
  {
    PowerControl_LowBatteryReject();
    return;
  }

  /* Boost stays on for the whole "board on" session, regardless of USB. */
  Dcdc_On();
  HAL_Delay(POWERON_SETTLE_MS);

  HAL_GPIO_WritePin(VOUT_CTRL_GPIO_Port, VOUT_CTRL_Pin, GPIO_PIN_SET);
  DebugLog_Printf("vout: enabled\r\n");

  /* New ON session. */
  s_ledOverridden = 0U;
  BattReport_Reset();
  LedIndicator_ShowPowerSource(usbPlugged);

  /* If the button that caused this power-on is still down, prime the button
     state machine so its release isn't reported as a new short press, and
     count the long-press timer from power-on. */
  if (PowerControl_IsButtonPressed() != 0U)
  {
    s_btnHeld               = 1U;
    s_btnPressStart         = HAL_GetTick();
    s_suppressNextBtnReport = 1U;
  }

  McuPower_SetState(MCU_POWER_ON);
}

void PowerControl_Init(void)
{
  PowerControl_PowerOnSequence();
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

  /* Only reached when USB keeps VEN up (otherwise the MCU is already dead). */
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

/* A 5s button hold keeps the MCU-initiated shutdown handshake. */
static void PowerControl_BeginButtonShutdown(void)
{
  McuPower_SetState(MCU_POWERING_OFF);
  s_powerOffRequested = 0U;
  s_shutdownLatched = 1U;
  AtProtocol_SendBtnLongPress();
  LedIndicator_BlinkWhite(2U);
  AtProtocol_SendPowerOffRequest();
  LedIndicator_RunShutdownFade(SHUTDOWN_FADE_DURATION_MS);
  PowerControl_CutPower();
}

static void PowerControl_BeginRemoteShutdown(void)
{
  DebugLog_Printf("power: Radxa AT+PWR0 -> shutdown\r\n");
  McuPower_SetState(MCU_POWERING_OFF);
  s_powerOffRequested = 0U;
  s_shutdownLatched = 1U;

  AtProtocol_SendPowerOffAck();
  LedIndicator_BlinkWhite(2U);
  LedIndicator_RunShutdownFade(SHUTDOWN_FADE_DURATION_MS);
  PowerControl_CutPower();
}

/* Times the button hold with HAL_GetTick() timestamps, so it works even
   though the loop is not polled on a fixed cadence. */
static void PowerControl_HandleButton(void)
{
  if (PowerControl_IsButtonPressed() != 0U)
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
        LedIndicator_BlinkWhite(1U); /* confirms the short press was recognized */
      }
      s_suppressNextBtnReport = 0U; /* only ever suppresses the one press */
    }
    s_btnHeld = 0U;
  }
}

static void PowerControl_CheckUsbPlugChange(void)
{
  uint8_t plugged = PowerControl_IsUsbPlugged();

  if ((s_usbInited == 0U) || (plugged != s_usbPlugged))
  {
    DebugLog_Printf("usb: %s\r\n", plugged ? "plugged" : "unplugged");
    if (plugged != 0U)
    {
      AtProtocol_SendUsbPlugIn();
    }
    else
    {
      AtProtocol_SendUsbPlugOut();
    }
    s_usbPlugged = plugged;
    s_usbInited  = 1U;

    /* Live GREEN<->BLUE switch, unless the main board overrode the LED. */
    if (s_ledOverridden == 0U)
    {
      LedIndicator_ShowPowerSource(plugged);
    }
  }
}

void PowerControl_NotifyLedOverridden(void)
{
  s_ledOverridden = 1U;
}

void PowerControl_Update(void)
{
  if (s_powerOffRequested != 0U)
  {
    PowerControl_BeginRemoteShutdown();
    return;
  }

  PowerControl_HandleButton();

  BattReport_ServiceAlert();
  BattReport_Service();

  if (SleepMode_ConsumeWakeTick() != 0U)
  {
    PowerControl_CheckUsbPlugChange();
    Dcdc_Update();
    BattReport_Tick1s();
  }

  /* Not tick-gated, so the breathing/blinking effect stays smooth.
     Suppressed once shutdown is latched so it doesn't overwrite the fade. */
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
