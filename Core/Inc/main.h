/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define BATT_ALT_Pin GPIO_PIN_15
#define BATT_ALT_GPIO_Port GPIOC
#define BTN_PWR_IN_Pin GPIO_PIN_0
#define BTN_PWR_IN_GPIO_Port GPIOA
#define USB_PWR_IN_Pin GPIO_PIN_6
#define USB_PWR_IN_GPIO_Port GPIOA
#define BATT_ADC_CTRL_Pin GPIO_PIN_7
#define BATT_ADC_CTRL_GPIO_Port GPIOA
#define USB_PLUG_IN_Pin GPIO_PIN_0
#define USB_PLUG_IN_GPIO_Port GPIOB
#define CHG_WKUP_Pin GPIO_PIN_12
#define CHG_WKUP_GPIO_Port GPIOB
#define LDO_OUT_Pin GPIO_PIN_13
#define LDO_OUT_GPIO_Port GPIOB
#define VOUT_CTRL_Pin GPIO_PIN_14
#define VOUT_CTRL_GPIO_Port GPIOB
#define PWM_CH1_Pin GPIO_PIN_8
#define PWM_CH1_GPIO_Port GPIOA
#define PWM_CH2_Pin GPIO_PIN_9
#define PWM_CH2_GPIO_Port GPIOA
#define PWM_CH3_Pin GPIO_PIN_10
#define PWM_CH3_GPIO_Port GPIOA
#define PWR_LED_Pin GPIO_PIN_11
#define PWR_LED_GPIO_Port GPIOA
#define BOOST_EN_Pin GPIO_PIN_15
#define BOOST_EN_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
