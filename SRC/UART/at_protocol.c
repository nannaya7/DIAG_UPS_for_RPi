/**
  ******************************************************************************
  * @file    at_protocol.c
  * @brief   AT-command line protocol between this MCU and the main board.
  *
  * Wire format: ASCII "AT+XXXX" lines terminated by \r and/or \n, matching
  * the style already used by DebugLog_Printf()/AtProtocol_Tx().
  *
  * Direction / commands:
  *  - Main board -> MCU: AT+RDY (boot complete)      -> MCU replies AT+OKAY.
  *  - MCU -> main board: AT+PWR0 (power-off request), AT+BTN0/BTN1 (short/
  *    long button press), AT+USB0/USB1 (plug in/out), AT+BATxx (SOC, 2-digit
  *    hex), AT+BVTxx (cell voltage, 2-digit decimal tenths of a volt),
  *    AT+BATL (battery-low alert) -- main board always replies AT+OKAY.
  *
  * Outbound notifications are fire-and-forget. A long-button shutdown also
  * sends AT+PWR0 but deliberately does not wait for a response because it is
  * the forced-off path used when the main board may be unresponsive.
  *
  * AT+BATL is new (added 2026-07-29, after the initial protocol spec).
  * (A periodic AT+PING heartbeat was added the same day and removed the
  * next day, 2026-07-30 -- decided unnecessary.)
  *
  * AT+LED,RR,GG,BB,PP (added 2026-09-08): main board -> MCU, inbound.
  * RR/GG/BB are hex PWM duty (00-FF, 0=off, FF=full brightness). PP is a
  * hex blink half-period in units of 100ms (00-FF -> 0-25.5s; each of
  * ON-time and OFF-time lasts PP*100ms, matching how the charge-status
  * breathe periods are specified) -- PP=00 means solid on, no blinking.
  * Only applied while MCU_STATUS is MCU_POWER_ON (the main board has no
  * power to send this otherwise, but checking explicitly means a stray
  * byte glitch during charge-check standby can't stomp on the SOC-based
  * breathing LED there) -- MCU replies AT+OKAY only when actually applied.
  * Also calls PowerControl_NotifyLedOverridden() so power_control.c's
  * USB-based default (GREEN on USB / BLUE on battery, see power_control.c)
  * stops overwriting this color on the next USB plug/unplug event, until
  * the next power-on session resets it.
  ******************************************************************************
  */
#include "at_protocol.h"
#include "uart2_comm.h"
#include "debug_log.h"
#include "rgb_led.h"
#include "mcu_power.h"
#include "power_control.h"
#include <string.h>

/* USART3 is now a transparent bridge to the ADuCM355 (uart_router.c) and the
   board has no commands of its own yet, so AT traffic is disabled: nothing is
   sent, and nothing calls AtProtocol_OnRxData(). To re-enable, set to 1, add
   uart2_comm.c back to the build, retarget it to USART3 through the router's
   TX ring, and route un-prefixed USART3 data to AtProtocol_OnRxData(). */
#define AT_PROTOCOL_ENABLED 0

#if AT_PROTOCOL_ENABLED
#define AtProtocol_Tx  Uart2Comm_Printf
#else
#define AtProtocol_Tx(...)  ((void)0)
#pragma diag_suppress 177 /* values only used by the (disabled) AtProtocol_Tx */
#endif

#define AT_LED_PREFIX       "AT+LED,"
#define AT_LED_PREFIX_LEN   7U
#define AT_LED_PARAMS_LEN   11U /* "RR,GG,BB,PP" */
#define AT_LED_PERIOD_UNIT_MS 100U

#define AT_LINE_BUF_SIZE  32U
static char     s_lineBuf[AT_LINE_BUF_SIZE];
static uint16_t s_lineLen = 0U;

/* Returns 0-15 for a hex digit, or 0xFF if c isn't one. */
static uint8_t AtProtocol_HexNibble(char c)
{
  if ((c >= '0') && (c <= '9')) { return (uint8_t)(c - '0'); }
  if ((c >= 'A') && (c <= 'F')) { return (uint8_t)(c - 'A' + 10); }
  if ((c >= 'a') && (c <= 'f')) { return (uint8_t)(c - 'a' + 10); }
  return 0xFFU;
}

/* Parses a 2-hex-digit byte at pair[0..1] into *value; returns 0 on success. */
static uint8_t AtProtocol_ParseHexByte(const char *pair, uint8_t *value)
{
  uint8_t hi = AtProtocol_HexNibble(pair[0]);
  uint8_t lo = AtProtocol_HexNibble(pair[1]);

  if ((hi == 0xFFU) || (lo == 0xFFU))
  {
    return 1U;
  }
  *value = (uint8_t)((hi << 4) | lo);
  return 0U;
}

/* Parses "AT+LED,RR,GG,BB,PP" (line already known to start with the
   prefix). Commas must fall exactly at the fixed positions. */
static void AtProtocol_HandleLedCommand(const char *line, uint16_t lineLen)
{
  const char *p = line + AT_LED_PREFIX_LEN;
  uint8_t red, green, blue, periodRaw;
  uint8_t bad = 0U;

  if ((lineLen != (AT_LED_PREFIX_LEN + AT_LED_PARAMS_LEN)) ||
      (p[2] != ',') || (p[5] != ',') || (p[8] != ','))
  {
    DebugLog_Printf("at: rx malformed LED '%s'\r\n", line);
    return;
  }

  bad |= AtProtocol_ParseHexByte(&p[0], &red);
  bad |= AtProtocol_ParseHexByte(&p[3], &green);
  bad |= AtProtocol_ParseHexByte(&p[6], &blue);
  bad |= AtProtocol_ParseHexByte(&p[9], &periodRaw);
  if (bad != 0U)
  {
    DebugLog_Printf("at: rx malformed LED '%s'\r\n", line);
    return;
  }

  if (McuPower_GetState() != MCU_POWER_ON)
  {
    /* Not powered on -- most likely a stray byte during charge-check
       standby, since the main board has no power to send this itself.
       Ignore it rather than stomping on the SOC-based breathing LED. */
    DebugLog_Printf("at: rx LED %02X,%02X,%02X,%02X ignored (not powered on)\r\n",
                     red, green, blue, periodRaw);
    return;
  }

  DebugLog_Printf("at: rx LED %02X,%02X,%02X,%02X -> tx OKAY\r\n", red, green, blue, periodRaw);
  RgbLed_SetEffect(RGBLED_EFFECT_BLINK, red, green, blue,
                    (uint32_t)periodRaw * AT_LED_PERIOD_UNIT_MS);
  PowerControl_NotifyLedOverridden();
  AtProtocol_Tx("AT+OKAY\r\n");
}

static void AtProtocol_HandleLine(const char *line, uint16_t lineLen)
{
  if (strcmp(line, "AT+PWR0") == 0)
  {
    /* Radxa decides when to shut down (e.g. based on reported SOC).
       Do not block this RX interrupt: the main loop sends AT+OKAY and
       runs the LED/output power-off sequence. */
    PowerControl_RequestPowerOff();
  }
  else if (strcmp(line, "AT+RDY") == 0)
  {
    DebugLog_Printf("at: rx RDY -> tx OKAY\r\n");
    AtProtocol_Tx("AT+OKAY\r\n");
  }
  else if (strcmp(line, "AT+OKAY") == 0)
  {
    /* Outbound messages do not wait for acknowledgements. */
  }
  else if (strncmp(line, AT_LED_PREFIX, AT_LED_PREFIX_LEN) == 0)
  {
    AtProtocol_HandleLedCommand(line, lineLen);
  }
  else
  {
    DebugLog_Printf("at: rx unknown '%s'\r\n", line);
  }
}

void AtProtocol_Init(void)
{
  s_lineLen      = 0U;
}

void AtProtocol_OnRxData(const uint8_t *data, uint16_t len)
{
  uint16_t i;

  for (i = 0U; i < len; i++)
  {
    char c = (char)data[i];

    if ((c == '\r') || (c == '\n'))
    {
      if (s_lineLen > 0U)
      {
        s_lineBuf[s_lineLen] = '\0';
        AtProtocol_HandleLine(s_lineBuf, s_lineLen);
        s_lineLen = 0U;
      }
    }
    else if (s_lineLen < (AT_LINE_BUF_SIZE - 1U))
    {
      s_lineBuf[s_lineLen++] = (char)c;
    }
    else
    {
      /* Line too long for the buffer: drop it and resync on the next
         terminator rather than overflow. */
      s_lineLen = 0U;
    }
  }
}

void AtProtocol_SendBtnShortPress(void) { AtProtocol_Tx("AT+BTN0\r\n"); }
void AtProtocol_SendBtnLongPress(void)  { AtProtocol_Tx("AT+BTN1\r\n"); }
void AtProtocol_SendUsbPlugIn(void)     { AtProtocol_Tx("AT+USB0\r\n"); }
void AtProtocol_SendUsbPlugOut(void)    { AtProtocol_Tx("AT+USB1\r\n"); }

void AtProtocol_SendBattery(uint8_t socPercent)
{
  uint8_t pct = (socPercent > 100U) ? 100U : socPercent;

  AtProtocol_Tx("AT+BAT%02X\r\n", pct);
}

void AtProtocol_SendBatteryLow(void) { AtProtocol_Tx("AT+BATL\r\n"); }

void AtProtocol_SendPowerOffAck(void) { AtProtocol_Tx("AT+OKAY\r\n"); }

void AtProtocol_SendPowerOffRequest(void) { AtProtocol_Tx("AT+PWR0\r\n"); }

void AtProtocol_SendBatteryVoltage(uint16_t voltageMv)
{
  uint16_t tenths = (uint16_t)((voltageMv + 50U) / 100U);

  if (tenths > 99U)
  {
    tenths = 99U;
  }
  AtProtocol_Tx("AT+BVT%02u\r\n", tenths);
}
