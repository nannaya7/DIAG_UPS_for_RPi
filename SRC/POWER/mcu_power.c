/**
  ******************************************************************************
  * @file    mcu_power.c
  * @brief   MCU power-state holder (see mcu_power.h). Every state change
  *          is logged.
  ******************************************************************************
  */
#include "mcu_power.h"
#include "debug_log.h"

static McuPower_State_t s_state = MCU_POWERING_ON;

const char *McuPower_StateName(McuPower_State_t state)
{
  switch (state)
  {
    case MCU_POWERING_ON:  return "POWERING_ON";
    case MCU_POWER_ON:     return "POWER_ON";
    case MCU_POWERING_OFF: return "POWERING_OFF";
    case MCU_POWER_OFF:    return "POWER_OFF";
    default:                return "?";
  }
}

void McuPower_SetState(McuPower_State_t state)
{
  if (state != s_state)
  {
    DebugLog_Printf("mcu_power: %s -> %s\r\n",
                     McuPower_StateName(s_state), McuPower_StateName(state));
    s_state = state;
  }
}

McuPower_State_t McuPower_GetState(void)
{
  return s_state;
}
