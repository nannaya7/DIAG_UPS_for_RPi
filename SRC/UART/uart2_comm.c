/**
  ******************************************************************************
  * @file    uart2_comm.c
  * @brief   USART2 link to the main board.
  *
  * RX uses HAL_UARTEx_ReceiveToIdle_DMA() into a circular DMA buffer
  * (hdma_usart2_rx = DMA1 Channel6, set up in stm32f1xx_hal_msp.c /
  * MX_DMA_Init()). HAL calls HAL_UARTEx_RxEventCallback() both on the
  * UART IDLE line (end of a burst) and when the circular buffer wraps,
  * with Size = the current write position in the buffer. We track our
  * own read position and forward newly-arrived bytes straight to
  * AtProtocol_OnRxData() (see at_protocol.c/.h, which assembles them into
  * AT+ command lines and handles the main-board protocol), splitting the
  * copy in two when the new data wraps past the end of the buffer.
  ******************************************************************************
  */
 
#include "uart2_comm.h"
#include "at_protocol.h"
#include <stdarg.h>
#include <stdio.h>

extern UART_HandleTypeDef huart2;

#define UART2_COMM_RX_BUF_SIZE  128U
#define UART2_COMM_TX_TIMEOUT_MS 100U

static uint8_t  s_rxBuf[UART2_COMM_RX_BUF_SIZE];
static uint16_t s_rxReadIdx = 0U;

void Uart2Comm_Init(void)
{
  HAL_UARTEx_ReceiveToIdle_DMA(&huart2, s_rxBuf, UART2_COMM_RX_BUF_SIZE);
  __HAL_DMA_DISABLE_IT(huart2.hdmarx, DMA_IT_HT); /* only care about IDLE / full-wrap events */
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  if (huart->Instance != USART2)
  {
    return;
  }

  if (Size != s_rxReadIdx)
  {
    if (Size > s_rxReadIdx)
    {
      /* linear: new data is one contiguous run */
      AtProtocol_OnRxData(&s_rxBuf[s_rxReadIdx], Size - s_rxReadIdx);
    }
    else
    {
      /* wrapped: new data runs to the end of the buffer, then from the start */
      AtProtocol_OnRxData(&s_rxBuf[s_rxReadIdx], UART2_COMM_RX_BUF_SIZE - s_rxReadIdx);
      if (Size > 0U)
      {
        AtProtocol_OnRxData(&s_rxBuf[0], Size);
      }
    }
  }

  s_rxReadIdx = Size;
}

HAL_StatusTypeDef Uart2Comm_Send(const uint8_t *data, uint16_t len, uint32_t timeout)
{
  return HAL_UART_Transmit(&huart2, (uint8_t *)data, len, timeout);
}

void Uart2Comm_Printf(const char *fmt, ...)
{
  char buf[128];
  va_list args;
  int len;

  va_start(args, fmt);
  len = vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  if (len > 0)
  {
    /* vsnprintf() returns the length the FULL formatted string would have
       needed, which can be larger than sizeof(buf) if it got truncated --
       clamp before sending, or we'd transmit stale stack bytes past what
       vsnprintf() actually wrote into buf[]. */
    if ((uint16_t)len > sizeof(buf) - 1U)
    {
      len = (int)sizeof(buf) - 1;
    }
    Uart2Comm_Send((uint8_t *)buf, (uint16_t)len, UART2_COMM_TX_TIMEOUT_MS);
  }
}
