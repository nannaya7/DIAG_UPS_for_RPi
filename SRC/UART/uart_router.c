/**
  ******************************************************************************
  * @file    uart_router.c
  * @brief   UART router (see uart_router.h).
  *
  * RX and TXE handling run in the USART2/USART3 ISRs at the same NVIC
  * priority, so they never preempt each other. Producers outside the ISRs
  * (UartRouter_SendToHost) mask interrupts around the ring push.
  ******************************************************************************
  */
#include "uart_router.h"

#define ROUTER_TX_BUF_SIZE   512U  /* power of two; holds two 256-byte packets */
#define ROUTER_IRQ_PRIORITY  5U

#define UART_ERR_FLAGS  (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)

typedef enum
{
  ROUTER_PORT_HOST = 0,  /* USART3: Raspberry Pi */
  ROUTER_PORT_ADUC,      /* USART2: ADuCM355     */
  ROUTER_PORT_COUNT
  /* ROUTER_PORT_NFC (USART1) is added here when NFC routing is needed. */
} RouterPort_t;

typedef struct
{
  USART_TypeDef *inst;
  IRQn_Type      irq;
  uint8_t        buf[ROUTER_TX_BUF_SIZE];
  volatile uint16_t head; /* written by producers */
  volatile uint16_t tail; /* written by the TXE ISR */
} RouterTxPort_t;

static RouterTxPort_t s_ports[ROUTER_PORT_COUNT] =
{
  [ROUTER_PORT_HOST] = { USART3, USART3_IRQn },
  [ROUTER_PORT_ADUC] = { USART2, USART2_IRQn },
};
static volatile uint32_t s_dropCount = 0U;

/* Queues one byte for transmission on 'port' and arms its TXE interrupt. */
static void Router_Push(RouterPort_t port, uint8_t byte)
{
  RouterTxPort_t *p = &s_ports[port];
  uint16_t next = (uint16_t)((p->head + 1U) & (ROUTER_TX_BUF_SIZE - 1U));

  if (next == p->tail)
  {
    s_dropCount++;
    return;
  }
  p->buf[p->head] = byte;
  p->head = next;
  p->inst->CR1 |= USART_CR1_TXEIE;
}

/* Routing rule: where a byte received on 'src' goes. Transparent bridge for
   now; prefix routing ([AD]/[NF]) and board-local commands are added here. */
static void UartRouter_Route(RouterPort_t src, uint8_t byte)
{
  switch (src)
  {
    case ROUTER_PORT_HOST: Router_Push(ROUTER_PORT_ADUC, byte); break;
    case ROUTER_PORT_ADUC: Router_Push(ROUTER_PORT_HOST, byte); break;
    default: break;
  }
}

static void Router_HandleIrq(RouterPort_t port)
{
  RouterTxPort_t *p = &s_ports[port];
  USART_TypeDef *u = p->inst;
  uint32_t sr = u->SR;

  if ((sr & UART_ERR_FLAGS) != 0U)
  {
    /* ORE/FE/NE/PE are cleared by SR read followed by DR read. A byte with
       a framing/noise error is discarded; ORE leaves the new byte in DR. */
    uint8_t d = (uint8_t)u->DR;
    if (((sr & USART_SR_ORE) != 0U) && ((sr & USART_SR_RXNE) != 0U))
    {
      UartRouter_Route(port, d);
    }
    return;
  }

  if ((sr & USART_SR_RXNE) != 0U)
  {
    UartRouter_Route(port, (uint8_t)u->DR);
  }

  if (((sr & USART_SR_TXE) != 0U) && ((u->CR1 & USART_CR1_TXEIE) != 0U))
  {
    if (p->tail != p->head)
    {
      u->DR = p->buf[p->tail];
      p->tail = (uint16_t)((p->tail + 1U) & (ROUTER_TX_BUF_SIZE - 1U));
    }
    else
    {
      u->CR1 &= ~USART_CR1_TXEIE;
    }
  }
}

void UartRouter_Init(void)
{
  uint32_t i;

  for (i = 0U; i < (uint32_t)ROUTER_PORT_COUNT; i++)
  {
    s_ports[i].head = 0U;
    s_ports[i].tail = 0U;
    s_ports[i].inst->CR1 |= USART_CR1_RXNEIE;
    HAL_NVIC_SetPriority(s_ports[i].irq, ROUTER_IRQ_PRIORITY, 0);
    HAL_NVIC_EnableIRQ(s_ports[i].irq);
  }
}

void UartRouter_IRQHandler(USART_TypeDef *instance)
{
  uint32_t i;

  for (i = 0U; i < (uint32_t)ROUTER_PORT_COUNT; i++)
  {
    if (s_ports[i].inst == instance)
    {
      Router_HandleIrq((RouterPort_t)i);
      return;
    }
  }
}

uint16_t UartRouter_SendToHost(const uint8_t *data, uint16_t len)
{
  uint16_t i;
  uint16_t queued = 0U;
  uint32_t primask = __get_PRIMASK();
  uint32_t dropBefore;

  __disable_irq();
  dropBefore = s_dropCount;
  for (i = 0U; i < len; i++)
  {
    Router_Push(ROUTER_PORT_HOST, data[i]);
  }
  queued = (uint16_t)(len - (s_dropCount - dropBefore));
  if (primask == 0U)
  {
    __enable_irq();
  }
  return queued;
}

uint8_t UartRouter_IsIdle(void)
{
  uint32_t i;

  for (i = 0U; i < (uint32_t)ROUTER_PORT_COUNT; i++)
  {
    if ((s_ports[i].head != s_ports[i].tail) ||
        ((s_ports[i].inst->SR & USART_SR_TC) == 0U))
    {
      return 0U;
    }
  }
  return 1U;
}

uint32_t UartRouter_GetDropCount(void)
{
  return s_dropCount;
}
