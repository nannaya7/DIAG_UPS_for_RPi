/**
  ******************************************************************************
  * @file    mcu_power.h
  * @brief   MCU power-state holder. The state transitions themselves are
  *          not decided here yet -- they'll be driven later by
  *          communication with the main board. For now this just stores
  *          the current mode and lets other modules read/set it.
  ******************************************************************************
  */
#ifndef __MCU_POWER_H
#define __MCU_POWER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  MCU_POWERING_ON = 0,
  MCU_POWER_ON,
  MCU_POWERING_OFF,
  MCU_POWER_OFF
} McuPower_State_t;

/**
  * @brief  Sets the current MCU power state. Logs the transition if the
  *         state actually changes.
  */
void McuPower_SetState(McuPower_State_t state);

/**
  * @retval Current MCU power state. Defaults to MCU_POWERING_ON at boot.
  */
McuPower_State_t McuPower_GetState(void);

/**
  * @retval Human-readable name for a McuPower_State_t.
  */
const char *McuPower_StateName(McuPower_State_t state);

#ifdef __cplusplus
}
#endif

#endif /* __MCU_POWER_H */
