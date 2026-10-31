/**
  ******************************************************************************
  * @file    power_control.h
  * @brief   UPS power sequencing: MCU self-latch, boost enable, output gate,
  *          power button handling.
  ******************************************************************************
  */
#ifndef __POWER_CONTROL_H
#define __POWER_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

typedef enum
{
  USB_STATUS_OUT = 0,
  USB_STATUS_IN
} UsbStatus_t;

/**
  * @brief  If BTN_PWR_IN isn't actually pressed (board booted from a USB
  *         plug event, not the button -- see file header), blocks in a
  *         charge-check standby loop (RGB LED breathing from MAX17048 SOC)
  *         until it is. Only then does it latch MCU power (LDO_OUT), and
  *         enable the battery boost (BOOST_EN) and the output switch to
  *         the counterpart board (VOUT_CTRL). Must be called once, after
  *         RgbLed_Init()/SleepMode_Init() (needs TIM1/TIM3 already
  *         running) and early in main() otherwise.
  */
void PowerControl_Init(void);

/**
  * @brief  Power/button state machine tick. Call once after every
  *         SleepMode_Enter() wake, not on a fixed cadence -- button
  *         hold-time uses HAL_GetTick() timestamps, and the periodic
  *         battery/DCDC/USB/heartbeat handling only runs on the 1Hz
  *         SleepMode_ConsumeWakeTick().
  *
  *         Note: a 5s long-press can block here for a while -- it runs the
  *         shutdown sequence, and if USB is still holding VEN up afterward
  *         (MCU didn't actually lose power), it drops straight into the
  *         same charge-check standby PowerControl_Init() uses at boot and
  *         only returns once a fresh button press re-powers the board.
  */
void UpdatePowerControl(void);

/** Queue the same shutdown as a 5s button hold. Safe from UART RX ISR.
    Only accepted while MCU_POWER_ON; executed by UpdatePowerControl(). */
void PowerControl_RequestPowerOff(void);

/**
  * @brief  Sends a short active-low pulse on CHG_WKUP to emulate a KEY press
  *         on the IP5310 power-bank IC. Idles HIGH; pulses LOW for ~500ms.
  *         Blocking. Called automatically on every power-button press edge
  *         from UpdatePowerControl(), but exposed here in case a manual
  *         trigger is ever needed too.
  */
void PowerControl_PulseChgWakeup(void);

/**
  * @retval Current USB plug status (IN/OUT), as last seen by
  *         PowerControl_CheckUsbPlugChange().
  */
UsbStatus_t PowerControl_GetUsbStatus(void);

/**
  * @brief  Tells power_control.c that the LED was just set by an AT+LED
  *         command (see at_protocol.c), so it stops applying its own
  *         power-on default (GREEN on USB / BLUE on battery) on USB
  *         plug/unplug events until the next power-on session. Call this
  *         right after RgbLed_SetEffect() when applying an AT+LED command.
  */
void PowerControl_NotifyLedOverridden(void);

/**
  * @retval Human-readable name for a UsbStatus_t ("IN"/"OUT").
  */
const char *PowerControl_UsbStatusName(UsbStatus_t status);

#ifdef __cplusplus
}
#endif

#endif /* __POWER_CONTROL_H */
