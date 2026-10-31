/**
******************************************************************************
* @file           : myapp.c
* @brief          : My Application
******************************************************************************
*
* Copyright (c) 2018 BENCH SOFT. CO. 
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without 
* modification, are permitted, provided that the following conditions are met:
*
* 1. Redistribution of source code must retain the above copyright notice, 
*    this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright notice,
*    this list of conditions and the following disclaimer in the documentation
*    and/or other materials provided with the distribution.
* 3. Neither the name of STMicroelectronics nor the names of other 
*    contributors to this software may be used to endorse or promote products 
*    derived from this software without specific written permission.
* 4. This software, including modifications and/or derivative works of this 
*    software, must execute solely and exclusively on microcontroller or
*    microprocessor devices manufactured by or for STMicroelectronics.
* 5. Redistribution and use of this software other than as permitted under 
*    this license is void and will automatically terminate your rights under 
*    this license. 
*
* THIS SOFTWARE IS PROVIDED BY BENCH SOFT. CO.  
* AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT 
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
* PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
* RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT 
* SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
******************************************************************************
*/
  
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
#include "power_control.h"
#include "max17048.h"
#include "debug_log.h"
#include "rgb_led.h"
#include "uart_router.h"
#include "sleep_mode.h"


//==============================================
// Start Main Scheduler
//==============================================

void App_Start(void)
{
  /* First, so the LED pins go to "off" (HIGH) before the boot delay. */
  RgbLed_Init();

  DebugLog_Init();
  Max17048_Init();

  HAL_Delay(1000); /* boot delay */

  /* TIM1 (LED PWM) and TIM3 (1Hz tick) must be running before
     PowerControl_Init(). */
  UartRouter_Init(); /* USART3 <-> USART2 transparent bridge */
  SleepMode_Init();

  DebugLog_Printf("\r\nDiagnostic UPS: boot\r\n");
  PowerControl_Init();

  /* SleepMode_Enter() (WFI) is not used yet: the loop free-runs so the LED
     breathing effect (HAL_GetTick() based) stays smooth. See "향후: 전원
     관리" in CLAUDE.md for the sleep requirements. */
  while (1)
  {
    PowerControl_Update();
  }
}

/* Note: assert_failed() (for USE_FULL_ASSERT) is defined once, in
   Core/Src/main.c -- not duplicated here. */

/************************ (C) COPYRIGHT BENCH SOFT. CO. *****END OF FILE****/

