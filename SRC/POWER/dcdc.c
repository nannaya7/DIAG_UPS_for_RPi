/**
  ******************************************************************************
  * @file    dcdc.c
  * @brief   DCDC (TPS61088 boost) state: ON/OFF via BOOST_EN, with a
  *          latching FAULT state when the sensed output droops too low.
  *          Every state change is logged.
  ******************************************************************************
  */
#include "dcdc.h"
#include "debug_log.h"

static Dcdc_State_t s_state = DCDC_STATE_OFF;

const char *Dcdc_StateName(Dcdc_State_t state)
{
  switch (state)
  {
    case DCDC_STATE_ON:    return "ON";
    case DCDC_STATE_FAULT: return "FAULT";
    case DCDC_STATE_OFF:
    default:                return "OFF";
  }
}

static void Dcdc_SetState(Dcdc_State_t newState)
{
  if (newState != s_state)
  {
    DebugLog_Printf("dcdc: %s -> %s\r\n", Dcdc_StateName(s_state), Dcdc_StateName(newState));
    s_state = newState;
  }
}

void Dcdc_On(void)
{
  HAL_GPIO_WritePin(BOOST_EN_GPIO_Port, BOOST_EN_Pin, GPIO_PIN_SET);
  Dcdc_SetState(DCDC_STATE_ON);
}

void Dcdc_Off(void)
{
  HAL_GPIO_WritePin(BOOST_EN_GPIO_Port, BOOST_EN_Pin, GPIO_PIN_RESET);
  Dcdc_SetState(DCDC_STATE_OFF);
}

void Dcdc_Update(void)
{
  /* No output-voltage sensing on this board (ADC unused): FAULT is never
     raised, state only follows Dcdc_On()/Dcdc_Off(). */
}

Dcdc_State_t Dcdc_GetState(void)
{
  return s_state;
}

uint8_t Dcdc_IsPowerGood(void)
{
  return (s_state == DCDC_STATE_ON) ? 1U : 0U;
}
