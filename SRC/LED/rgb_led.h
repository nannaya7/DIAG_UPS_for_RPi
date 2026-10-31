/**
  ******************************************************************************
  * @file    rgb_led.h
  * @brief   RGB status LED array (LED2/4/5/6/7/8/9) PWM control over TIM1.
  *          PA8/TIM1_CH1 = RED, PA9/TIM1_CH2 = GREEN, PA10/TIM1_CH3 = BLUE.
  ******************************************************************************
  */
#ifndef __RGB_LED_H
#define __RGB_LED_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

typedef enum
{
  RGBLED_EFFECT_BREATHE = 0,  /* smooth ramp up then down, repeats forever  */
  RGBLED_EFFECT_BLINK,        /* hard on/off toggle at the given brightness */
  RGBLED_EFFECT_FADE_OUT,     /* one-shot ramp from peak to off, then stays off */
  RGBLED_EFFECT_PULSE         /* short ON pulse once per period (RgbLed_SetPulse) */
} RgbLedEffect_t;

/**
  * @brief  Starts PWM on TIM1 CH1/CH2/CH3 and turns the LED off.
  *         Call once, after MX_TIM1_Init().
  */
void RgbLed_Init(void);

/**
  * @brief  Sets the LED color immediately, bypassing the effect engine.
  * @param  red/green/blue: brightness 0 (off) .. 255 (full brightness).
  */
void RgbLed_SetColor(uint8_t red, uint8_t green, uint8_t blue);

/**
  * @brief  Convenience for RgbLed_SetColor(0, 0, 0).
  */
void RgbLed_Off(void);

/**
  * @brief  Selects the running effect and (re)starts its cycle from now.
  *         Only call this when the effect/color/period actually changes --
  *         calling it every tick with the same params restarts the ramp
  *         and looks like a stutter.
  * @param  effect       RGBLED_EFFECT_BREATHE, _BLINK, or _FADE_OUT.
  * @param  red/green/blue: peak brightness 0..255.
  * @param  halfPeriodMs Meaning depends on effect: BREATHE = duration of one
  *                      ramp phase (up or down); BLINK = ON time (OFF time
  *                      is the same); FADE_OUT = total one-shot fade
  *                      duration, after which the LED stays off. 0 means
  *                      solid on at (red,green,blue), no animation -- matches
  *                      the AT+LED "period=00 -> full ON" spec (BREATHE/BLINK
  *                      only; FADE_OUT is always meant to be called with a
  *                      nonzero duration).
  */
void RgbLed_SetEffect(RgbLedEffect_t effect, uint8_t red, uint8_t green,
                       uint8_t blue, uint32_t halfPeriodMs);

/**
  * @brief  Starts a pulse effect: LED on at (red,green,blue) for onMs at the
  *         start of every periodMs, off for the rest. Same restart caveat as
  *         RgbLed_SetEffect().
  */
void RgbLed_SetPulse(uint8_t red, uint8_t green, uint8_t blue,
                      uint32_t onMs, uint32_t periodMs);

/**
  * @brief  Advances the currently-selected effect based on HAL_GetTick()
  *         and writes the resulting color. Call as often as possible (the
  *         main loop no longer sleeps) for smooth breathing/blinking.
  */
void RgbLed_Update(void);

#ifdef __cplusplus
}
#endif

#endif /* __RGB_LED_H */
