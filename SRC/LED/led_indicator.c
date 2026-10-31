/**
  ******************************************************************************
  * @file    led_indicator.c
  * @brief   Status LED patterns (see led_indicator.h).
  ******************************************************************************
  */
#include "led_indicator.h"
#include "rgb_led.h"

#define LED_BREATHE_HALF_PERIOD_MS 3000U /* 3s brighten + 3s dim = 6s cycle */
#define LED_BREATHE_PEAK           255U

#define POWER_SRC_LED_PEAK         128U   /* GRN/BLU at ~50% brightness */
#define POWER_SRC_PULSE_ON_MS      300U   /* lit for 0.3s ...            */
#define POWER_SRC_PULSE_PERIOD_MS  5000U  /* ... once every 5s           */

#define BTN_BLINK_WHITE_PEAK       128U  /* ~50% brightness -- full 255 was too bright */
#define BTN_BLINK_ON_MS            150U
#define BTN_BLINK_OFF_MS           150U

#define LOWBATT_BLINK_RED_PEAK     255U
#define LOWBATT_BLINK_ON_MS        500U
#define LOWBATT_BLINK_OFF_MS       500U
#define LOWBATT_BLINK_COUNT        3U

#define SHUTDOWN_LED_PEAK          255U  /* GREEN+RED fade-out starting brightness */
#define SHUTDOWN_FADE_POLL_MS      5U

void LedIndicator_ShowPowerSource(uint8_t usbPlugged)
{
  if (usbPlugged != 0U)
  {
    RgbLed_SetPulse(0U, POWER_SRC_LED_PEAK, 0U, POWER_SRC_PULSE_ON_MS, POWER_SRC_PULSE_PERIOD_MS);
  }
  else
  {
    RgbLed_SetPulse(0U, 0U, POWER_SRC_LED_PEAK, POWER_SRC_PULSE_ON_MS, POWER_SRC_PULSE_PERIOD_MS);
  }
}

void LedIndicator_ShowChargeStatus(uint8_t chargeComplete)
{
  if (chargeComplete != 0U)
  {
    /* halfPeriodMs=0 -> RgbLed_Update()'s "solid on" branch, so the LED
       stays off instead of being fought over by the effect animation. */
    RgbLed_SetEffect(RGBLED_EFFECT_BREATHE, 0U, 0U, 0U, 0U);
  }
  else
  {
    RgbLed_SetEffect(RGBLED_EFFECT_BREATHE, LED_BREATHE_PEAK, 0U, 0U, LED_BREATHE_HALF_PERIOD_MS);
  }
}

void LedIndicator_BlinkWhite(uint8_t times)
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

void LedIndicator_BlinkLowBattery(void)
{
  uint8_t i;

  for (i = 0U; i < LOWBATT_BLINK_COUNT; i++)
  {
    RgbLed_SetColor(LOWBATT_BLINK_RED_PEAK, 0U, 0U);
    HAL_Delay(LOWBATT_BLINK_ON_MS);
    RgbLed_Off();
    HAL_Delay(LOWBATT_BLINK_OFF_MS);
  }
}

void LedIndicator_RunShutdownFade(uint32_t durationMs)
{
  uint32_t start;

  RgbLed_SetEffect(RGBLED_EFFECT_FADE_OUT, SHUTDOWN_LED_PEAK,
                   SHUTDOWN_LED_PEAK, 0U, durationMs);
  start = HAL_GetTick();
  while ((HAL_GetTick() - start) < durationMs)
  {
    RgbLed_Update();
    HAL_Delay(SHUTDOWN_FADE_POLL_MS);
  }
  RgbLed_Off();
}
