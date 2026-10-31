/**
  ******************************************************************************
  * @file    uart_router.h
  * @brief   UART3 (Raspberry Pi) <-> UART2 (ADuCM355) transparent bridge.
  *
  * Interrupt-driven, no DMA: every received byte is pushed straight into the
  * peer port's TX ring buffer from the RX interrupt.
  *   USART3 RX -> USART2 TX
  *   USART2 RX -> USART3 TX
  * USART1 (NFC) is initialised by CubeMX but not routed yet.
  *
  * Future: "[AD]" / "[NF]" prefix routing on USART3 -- see CLAUDE.md.
  ******************************************************************************
  */
#ifndef __UART_ROUTER_H
#define __UART_ROUTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/** @brief Enables RX interrupts / NVIC for USART2 and USART3. Call once after MX_USARTx_Init(). */
void UartRouter_Init(void);

/** @brief Call from USART2_IRQHandler / USART3_IRQHandler. */
void UartRouter_IRQHandler(USART_TypeDef *instance);

/** @brief Bytes dropped because a TX ring was full (debug). */
uint32_t UartRouter_GetDropCount(void);

#ifdef __cplusplus
}
#endif

#endif /* __UART_ROUTER_H */
