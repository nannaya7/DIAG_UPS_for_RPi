/**
  ******************************************************************************
  * @file    uart2_comm.h
  * @brief   USART2 link to the main board: circular-DMA RX buffer with
  *          IDLE-line framing, fed into a receive-data parsing function.
  ******************************************************************************
  */
#ifndef __UART2_COMM_H
#define __UART2_COMM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
  * @brief  Starts circular-DMA + IDLE-line reception on USART2.
  *         Call once, after MX_DMA_Init()/MX_USART2_UART_Init().
  */
void Uart2Comm_Init(void);

/**
  * @brief  Blocking send of raw bytes over USART2.
  * @param  data    Buffer to send.
  * @param  len     Number of bytes.
  * @param  timeout HAL_UART_Transmit timeout in ms.
  * @retval HAL status.
  */
HAL_StatusTypeDef Uart2Comm_Send(const uint8_t *data, uint16_t len, uint32_t timeout);

/**
  * @brief  printf-style blocking send over USART2 (max 128 bytes formatted).
  */
void Uart2Comm_Printf(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* __UART2_COMM_H */
