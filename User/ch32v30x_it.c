/********************************** (C) COPYRIGHT *******************************
* File Name          : ch32v30x_it.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2021/06/06
* Description        : Main Interrupt Service Routines.
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
* SPDX-License-Identifier: Apache-2.0
*******************************************************************************/
#include "ch32v30x_it.h"

void NMI_Handler(void) __attribute__((interrupt()));
void HardFault_Handler(void) __attribute__((interrupt()));

/*********************************************************************
 * @fn      NMI_Handler
 *
 * @brief   This function handles NMI exception.
 *
 * @return  none
 */
void NMI_Handler(void)
{
}

/*********************************************************************
 * @fn      HardFault_Handler
 *
 * @brief   This function handles Hard Fault exception.
 *
 * @return  none
 */
void HardFault_Handler(void)
{
  uint32_t mepc = 0;
  uint32_t mcause = 0;
  uint32_t mtval = 0;
  mepc =  __get_MEPC();

mcause = __get_MCAUSE();

mtval = __get_MTVAL();
printf("mepc :%08x\r\n", mepc);

printf("mcause:%08x\r\n", mcause);

printf("mtval :%08x\r\n", mtval);
  while (1)
  {
  }
}



