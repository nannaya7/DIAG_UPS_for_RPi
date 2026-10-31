/**
  ******************************************************************************
  * @file    led_indicator.h
  * @brief   Status LED patterns used by the power sequencing code, built on
  *          the rgb_led effect engine.
  ******************************************************************************
  */
#ifndef __LED_INDICATOR_H
#define __LED_INDICATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
  * @brief  Power-on default: GREEN pulse (0.3s every 5s) while USB supplies
  *         power, BLUE pulse while running on battery alone.
  */
void LedIndicator_ShowPowerSource(uint8_t usbPlugged);

/**
  * @brief  Charge-check standby: RED breathing while charging, LED off once
  *         charge is complete.
  */
void LedIndicator_ShowChargeStatus(uint8_t chargeComplete);

/**
  * @brief  Blocking white blink(s) confirming a button press. Bypasses the
  *         running effect, which resumes on the next RgbLed_Update().
  */
void LedIndicator_BlinkWhite(uint8_t times);

/** @brief Blocking RED blink(s) signalling a refused battery-only power-on. */
void LedIndicator_BlinkLowBattery(void);

/** @brief Blocking GREEN+RED fade-out to off over durationMs (shutdown). */
void LedIndicator_RunShutdownFade(uint32_t durationMs);

#ifdef __cplusplus
}
#endif

#endif /* __LED_INDICATOR_H */
