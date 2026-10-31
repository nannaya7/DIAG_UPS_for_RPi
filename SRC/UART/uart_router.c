/**
  ******************************************************************************
  * @file    uart_router.c
  * @brief   UART3 <-> UART2 transparent bridge (see uart_router.h).
  *
  * All ring-buffer producers and consumers run in USART2/USART3 ISRs at the
  * same NVIC priority, so they never preempt each other and no locking is
  * needed. A producer outside these ISRs (e.g. a future main-loop sender)
  * must disable interrupts around its push.
  ******************************************************************************
  */
#include "uart_router.h"

#define ROUTER_TX_BUF_SIZE   512U  /* power of two; holds two 256-byte packets */
#define ROUTER_IRQ_PRIORITY  5U

#define UART_ERR_FLAGS  (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)

typedef struct
{
  USART_TypeDef *inst;
  IRQn_Type      irq;
  uint8_t        buf[ROUTER_TX_BUF_SIZE];
  volatile uint16_t head; /* written by producer (peer RX ISR) */
  volatile uint16_t tail; /* written by consumer (own TXE ISR) */
} RouterPort_t;

static RouterPort_t s_uart2 = { USART2, USART2_IRQn };
static RouterPort_t s_uart3 = { USART3, USART3_IRQn };
static volatile uint32_t s_dropCount = 0U;

/* Queues one byte for transmission on 'port' and arms its TXE interrupt. */
static void Router_Push(RouterPort_t *port, uint8_t byte)
{
  uint16_t next = (uint16_t)((port->head + 1U) & (ROUTER_TX_BUF_SIZE - 1U));

  if (next == port->tail)
  {
    s_dropCount++;
    return;
  }
  port->buf[port->head] = byte;
  port->head = next;
  port->inst->CR1 |= USART_CR1_TXEIE;
}

static void Router_HandleIrq(RouterPort_t *self, RouterPort_t *peer)
{
  USART_TypeDef *u = self->inst;
  uint32_t sr = u->SR;

  if ((sr & UART_ERR_FLAGS) != 0U)
  {
    /* ORE/FE/NE/PE are cleared by SR read followed by DR read. A byte with
       a framing/noise error is discarded; ORE leaves the new byte in DR. */
    uint8_t d = (uint8_t)u->DR;
    if (((sr & USART_SR_ORE) != 0U) && ((sr & USART_SR_RXNE) != 0U))
    {
      Router_Push(peer, d);
    }
    return;
  }

  if ((sr & USART_SR_RXNE) != 0U)
  {
    Router_Push(peer, (uint8_t)u->DR);
  }

  if (((sr & USART_SR_TXE) != 0U) && ((u->CR1 & USART_CR1_TXEIE) != 0U))
  {
    if (self->tail != self->head)
    {
      u->DR = self->buf[self->tail];
      self->tail = (uint16_t)((self->tail + 1U) & (ROUTER_TX_BUF_SIZE - 1U));
    }
    else
    {
      u->CR1 &= ~USART_CR1_TXEIE;
    }
  }
}

void UartRouter_Init(void)
{
  s_uart2.head = s_uart2.tail = 0U;
  s_uart3.head = s_uart3.tail = 0U;

  USART2->CR1 |= USART_CR1_RXNEIE;
  USART3->CR1 |= USART_CR1_RXNEIE;

  HAL_NVIC_SetPriority(USART2_IRQn, ROUTER_IRQ_PRIORITY, 0);
  HAL_NVIC_EnableIRQ(USART2_IRQn);
  HAL_NVIC_SetPriority(USART3_IRQn, ROUTER_IRQ_PRIORITY, 0);
  HAL_NVIC_EnableIRQ(USART3_IRQn);
}

void UartRouter_IRQHandler(USART_TypeDef *instance)
{
  if (instance == USART3)
  {
    Router_HandleIrq(&s_uart3, &s_uart2);
  }
  else if (instance == USART2)
  {
    Router_HandleIrq(&s_uart2, &s_uart3);
  }
}

uint32_t UartRouter_GetDropCount(void)
{
  return s_dropCount;
}
