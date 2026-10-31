/**
  ******************************************************************************
  * @file    uart_router.h
  * @brief   UART router: HOST (USART3, Raspberry Pi) <-> ADUC (USART2,
  *          ADuCM355) transparent bridge. NFC (USART1) is reserved.
  *
  * Interrupt-driven, no DMA. Every received byte goes through the routing
  * rule and is pushed into the destination port's TX ring buffer:
  *   HOST RX -> ADUC TX
  *   ADUC RX -> HOST TX
  * Future: "[AD]" / "[NF]" prefix routing on HOST RX, un-prefixed HOST data
  * handled by the board itself -- see CLAUDE.md. The only place that needs
  * to change for that is UartRouter_Route() in uart_router.c.
  ******************************************************************************
  */
#ifndef __UART_ROUTER_H
#define __UART_ROUTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/** @brief Enables RX interrupts / NVIC for the routed ports. Call once after the MX_USARTx_Init() calls. */
void UartRouter_Init(void);

/** @brief Call from USART2_IRQHandler / USART3_IRQHandler. */
void UartRouter_IRQHandler(USART_TypeDef *instance);

/**
  * @brief  Queues bytes for the HOST (Raspberry Pi) port, e.g. board replies.
  *         Safe from the main loop (interrupts are masked around the push).
  *         Bytes that don't fit in the TX ring are dropped and counted.
  * @retval Number of bytes queued.
  */
uint16_t UartRouter_SendToHost(const uint8_t *data, uint16_t len);

/** @retval 1 if every TX ring is empty and the last byte left the shifter (safe to sleep). */
uint8_t UartRouter_IsIdle(void);

/** @brief Bytes dropped because a TX ring was full (debug). */
uint32_t UartRouter_GetDropCount(void);

#ifdef __cplusplus
}
#endif

#endif /* __UART_ROUTER_H */
