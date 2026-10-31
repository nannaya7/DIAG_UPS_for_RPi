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
  DebugLog_Init();
  Max17048_Init();

  HAL_Delay(1000); /* Boot Delay 500ms */

  /* RgbLed_Init()/SleepMode_Init() must run before PowerControl_Init():
     it may block in a charge-check standby loop (USB-only boot, no
     button press yet) that drives the LED effect engine and polls
     SleepMode_ConsumeWakeTick(), both of which need TIM1/TIM3 already
     running. */
  RgbLed_Init();
  UartRouter_Init(); /* USART3 <-> USART2 transparent bridge */
  SleepMode_Init();

  /* Logged here, before PowerControl_Init(), because that call can now
     block for a long time in charge-check standby (USB-only boot, no
     button press yet) -- an RTT log watcher should see the boot banner
     right away, not only once someone eventually presses the button. */
  DebugLog_Printf("\r\ncappic UPS: boot\r\n");
  PowerControl_Init();

  /* 2026-09-08: SleepMode_Enter() (WFI) temporarily disabled -- keeps SWD
     attached and the loop free-running so the LED breathing/blinking
     effect (driven by HAL_GetTick(), needs finer-than-1Hz timing) stays
     smooth. TIM3's 1Hz tick is untouched, so existing once-a-second logic
     in UpdatePowerControl() still runs normally. */
  while (1)
  {
    UpdatePowerControl();
  }
}

/* Note: assert_failed() (for USE_FULL_ASSERT) is defined once, in
   Core/Src/main.c -- not duplicated here. */

/************************ (C) COPYRIGHT BENCH SOFT. CO. *****END OF FILE****/

