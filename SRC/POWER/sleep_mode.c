/**
  ******************************************************************************
  * @file    sleep_mode.c
  * @brief   Cortex-M SLEEP mode (WFI) with a TIM3-driven 1Hz wake tick.
  *
  * TIM3 is a plain general-purpose timer not used anywhere else in this
  * project (TIM1 is taken by the RGB LED PWM); it's owned entirely by
  * this module, including its own IRQ handler below, so nothing else
  * needs to change to add it.
  *
  * TIM3 clock = 64MHz (APB1 timer clock, see .ioc RCC config).
  * Prescaler 6400, Period 10000 -> 10kHz / 10000 = 1.000s exactly.
  ******************************************************************************
  */
#include "sleep_mode.h"

#define SLEEP_TICK_PRESCALER  6399U  /* 64MHz / 6400 = 10kHz timer tick */
#define SLEEP_TICK_PERIOD     9999U  /* 10kHz / 10000 counts = 1.000s   */

static TIM_HandleTypeDef htim3;
static volatile uint8_t  s_wakeTick = 0U;

void SleepMode_Init(void)
{
  __HAL_RCC_TIM3_CLK_ENABLE();

  htim3.Instance = TIM3;
  htim3.Init.Prescaler = SLEEP_TICK_PRESCALER;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = SLEEP_TICK_PERIOD;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  HAL_TIM_Base_Init(&htim3);

  HAL_NVIC_SetPriority(TIM3_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(TIM3_IRQn);

  HAL_TIM_Base_Start_IT(&htim3);
}

void SleepMode_Enter(void)
{
  HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
}

uint8_t SleepMode_ConsumeWakeTick(void)
{
  uint8_t tick = s_wakeTick;
  s_wakeTick = 0U;
  return tick;
}

void TIM3_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&htim3);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM3)
  {
    s_wakeTick = 1U;
  }
}
