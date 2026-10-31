/**
  ******************************************************************************
  * @file    dcdc.h
  * @brief   DCDC (TPS61088 boost) state: ON/OFF via BOOST_EN, with a
  *          latching FAULT state when the sensed output droops too low.
  ******************************************************************************
  */
#ifndef __DCDC_H
#define __DCDC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

typedef enum
{
  DCDC_STATE_OFF = 0,
  DCDC_STATE_ON,
  DCDC_STATE_FAULT
} Dcdc_State_t;

/**
  * @brief  Drives BOOST_EN HIGH and sets the state to ON.
  */
void Dcdc_On(void);

/**
  * @brief  Drives BOOST_EN LOW and sets the state to OFF (clears any FAULT).
  */
void Dcdc_Off(void);

/**
  * @brief  While ON, checks DCDC_ADC and latches FAULT if it reads at or
  *         below 4.7V. Called once per 1Hz wake tick from
  *         UpdatePowerControl(). Once latched, FAULT only clears via
  *         Dcdc_On()/Dcdc_Off().
  */
void Dcdc_Update(void);

/**
  * @retval Current DCDC state (OFF/ON/FAULT).
  */
Dcdc_State_t Dcdc_GetState(void);

/**
  * @retval 1 if the DCDC is ON and not FAULTed, 0 otherwise (OFF or FAULT).
  */
uint8_t Dcdc_IsPowerGood(void);

/**
  * @retval Human-readable name for a Dcdc_State_t ("OFF"/"ON"/"FAULT").
  */
const char *Dcdc_StateName(Dcdc_State_t state);

#ifdef __cplusplus
}
#endif

#endif /* __DCDC_H */
