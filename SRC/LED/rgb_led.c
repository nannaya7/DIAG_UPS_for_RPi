/**
  ******************************************************************************
  * @file    rgb_led.c
  * @brief   RGB status LED array (LED2/4/5/6/7/8/9) PWM control over TIM1.
  *
  * 2026-09-08 hardware rework (schematic v1.1, Sheet2_UPS_LED): the single
  * common-anode LED2 was replaced by 7 parallel common-anode RGB LEDs
  * (LED2/4/5/6/7/8/9, anodes tied to VMCU, each with its own per-color
  * ballast resistor into a shared RED/GRN/BLU rail) so the combined
  * cathode current no longer fits through a GPIO pin directly. Each color
  * rail is now switched to GND by its own low-side N-FET (Q4/Q5/Q6,
  * SI2302S) gated by TIM1 CH1/CH2/CH3 (PWM_CH1/2/3 = PA8/PA9/PA10).
  * Gate HIGH turns the FET on (LED lit) -- the OPPOSITE sense of the old
  * direct-drive cathode sink -- so CCR now maps straight to brightness,
  * no inversion. TIM1's OCPolarity stays at CubeMX's default (active
  * HIGH) so main.c is untouched by regeneration.
  ******************************************************************************
  */
#include "rgb_led.h"

extern TIM_HandleTypeDef htim1;

#define RGB_LED_CHANNEL_RED    TIM_CHANNEL_1
#define RGB_LED_CHANNEL_GREEN  TIM_CHANNEL_2
#define RGB_LED_CHANNEL_BLUE   TIM_CHANNEL_3
#define RGB_LED_BRIGHTNESS_MAX 255U

static RgbLedEffect_t s_effect          = RGBLED_EFFECT_BLINK;
static uint8_t        s_effectRed       = 0U;
static uint8_t        s_effectGreen     = 0U;
static uint8_t        s_effectBlue      = 0U;
static uint32_t       s_halfPeriodMs    = 0U;   /* 0 = solid, no animation */
static uint32_t       s_effectStartTick = 0U;

static uint32_t RgbLed_BrightnessToCcr(uint8_t brightness)
{
  uint32_t period = htim1.Init.Period;

  /* Low-side N-FET (Q4/Q5/Q6) per color rail: gate HIGH turns it on and
     lights the LEDs, so more ON-time = brighter -- straight mapping,
     no inversion (see file header). */
  return (period * (uint32_t)brightness) / RGB_LED_BRIGHTNESS_MAX;
}

void RgbLed_Init(void)
{
  HAL_TIM_PWM_Start(&htim1, RGB_LED_CHANNEL_RED);
  HAL_TIM_PWM_Start(&htim1, RGB_LED_CHANNEL_GREEN);
  HAL_TIM_PWM_Start(&htim1, RGB_LED_CHANNEL_BLUE);

  RgbLed_Off();
}

void RgbLed_SetColor(uint8_t red, uint8_t green, uint8_t blue)
{
  __HAL_TIM_SET_COMPARE(&htim1, RGB_LED_CHANNEL_RED, RgbLed_BrightnessToCcr(red));
  __HAL_TIM_SET_COMPARE(&htim1, RGB_LED_CHANNEL_GREEN, RgbLed_BrightnessToCcr(green));
  __HAL_TIM_SET_COMPARE(&htim1, RGB_LED_CHANNEL_BLUE, RgbLed_BrightnessToCcr(blue));
}

void RgbLed_Off(void)
{
  RgbLed_SetColor(0U, 0U, 0U);
}

void RgbLed_SetEffect(RgbLedEffect_t effect, uint8_t red, uint8_t green,
                       uint8_t blue, uint32_t halfPeriodMs)
{
  s_effect          = effect;
  s_effectRed       = red;
  s_effectGreen     = green;
  s_effectBlue      = blue;
  s_halfPeriodMs    = halfPeriodMs;
  s_effectStartTick = HAL_GetTick();
}

void RgbLed_Update(void)
{
  uint32_t elapsed;
  uint32_t phase;
  uint32_t linear;
  uint32_t gamma;

  if (s_halfPeriodMs == 0U)
  {
    /* period=0 -> solid on, no animation (matches AT+LED spec). */
    RgbLed_SetColor(s_effectRed, s_effectGreen, s_effectBlue);
    return;
  }

  if (s_effect == RGBLED_EFFECT_FADE_OUT)
  {
    /* One-shot ramp down, unlike BREATHE/BLINK this never loops -- elapsed
       is clamped (not wrapped) so brightness sits at 0 forever once the
       fade duration has passed instead of jumping back to peak. Same
       squared-gamma shaping as BREATHE so the fade looks perceptually
       even instead of dropping off too fast near the end. */
    elapsed = HAL_GetTick() - s_effectStartTick;
    phase   = (elapsed < s_halfPeriodMs) ? elapsed : s_halfPeriodMs;
    linear  = 255U - ((phase * 255U) / s_halfPeriodMs);
    gamma   = (linear * linear) / 255U;

    RgbLed_SetColor((uint8_t)(((uint32_t)s_effectRed   * gamma) / 255U),
                     (uint8_t)(((uint32_t)s_effectGreen * gamma) / 255U),
                     (uint8_t)(((uint32_t)s_effectBlue  * gamma) / 255U));
    return;
  }

  elapsed = (HAL_GetTick() - s_effectStartTick) % (s_halfPeriodMs * 2U);

  if (s_effect == RGBLED_EFFECT_BLINK)
  {
    uint8_t on = (elapsed < s_halfPeriodMs) ? 1U : 0U;
    RgbLed_SetColor(on ? s_effectRed : 0U, on ? s_effectGreen : 0U, on ? s_effectBlue : 0U);
    return;
  }

  /* RGBLED_EFFECT_BREATHE: ramp 0..halfPeriodMs up, then back down. A
     straight linear ramp on PWM duty looks wrong to the eye -- human
     brightness perception is roughly logarithmic, so equal PWM steps near
     the bottom barely register while steps near the top look like a
     sudden flare. Squaring the linear time-position (then rescaling back
     to 0..255) is a cheap integer-only stand-in for gamma correction
     (~gamma 2) that reads as a much more even fade. Every intermediate
     value here stays within 0..255*255, so this can't overflow uint32_t. */
  phase  = (elapsed < s_halfPeriodMs) ? elapsed : ((s_halfPeriodMs * 2U) - elapsed);
  linear = (phase * 255U) / s_halfPeriodMs;   /* 0..255 time position   */
  gamma  = (linear * linear) / 255U;          /* 0..255 perceptual scale */

  RgbLed_SetColor((uint8_t)(((uint32_t)s_effectRed   * gamma) / 255U),
                   (uint8_t)(((uint32_t)s_effectGreen * gamma) / 255U),
                   (uint8_t)(((uint32_t)s_effectBlue  * gamma) / 255U));
}
