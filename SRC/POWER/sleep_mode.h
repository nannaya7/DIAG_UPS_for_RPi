/**
  ******************************************************************************
  * @file    sleep_mode.h
  * @brief   Cortex-M SLEEP mode (WFI) with a TIM3-driven 1Hz wake tick.
  *
  * SLEEP mode only clock-gates the CPU core -- all peripherals (USART,
  * EXTI, TIM3) keep running and any of their enabled interrupts wakes the
  * core: USART2/USART3 RX and the PA0 button (EXTI0) are regular NVIC
  * interrupts. TIM3 is the deliberate ~1000ms tick for battery/USB handling.
  *
  ******************************************************************************
  */
#ifndef __SLEEP_MODE_H
#define __SLEEP_MODE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
  * @brief  Starts the TIM3 1Hz wake-tick timer. Call once at boot.
  */
void SleepMode_Init(void);

/**
  * @brief  Enters SLEEP mode (WFI) until any enabled interrupt occurs.
  */
void SleepMode_Enter(void);

/**
  * @brief  Returns non-zero and clears the flag if the 1-second TIM3 tick
  *         fired since the last call.
  */
uint8_t SleepMode_ConsumeWakeTick(void);

#ifdef __cplusplus
}
#endif

#endif /* __SLEEP_MODE_H */
