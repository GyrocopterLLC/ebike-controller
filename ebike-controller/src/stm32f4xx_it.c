/**
 ******************************************************************************
 * @file    USB_Device/CDC_Standalone/Src/stm32f4xx_it.c
 * @author  MCD Application Team
 * @version V1.2.0
 * @date    26-December-2014
 * @brief   Main Interrupt Service Routines.
 *          This file provides template for all exceptions handler and
 *          peripherals interrupt service routine.
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; COPYRIGHT(c) 2014 STMicroelectronics</center></h2>
 *
 * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *        http://www.st.com/software_license_agreement_liberty_v2
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f4xx_it.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

/* extern PCD_HandleTypeDef hpcd; */

/* UART handler declared in "usbd_cdc_interface.c" file */
//extern UART_HandleTypeDef UartHandle;
/* TIM handler declared in "usbd_cdc_interface.c" file */
//extern TIM_HandleTypeDef TimHandle;
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/******************************************************************************/
/*             Cortex-M4 Processor Exceptions Handlers                        */
/******************************************************************************/

/**
 * @brief  This function handles NMI exception.
 * @param  None
 * @retval None
 */
void NMI_Handler(void) {
}

/**
 * @brief  This function handles Hard Fault exception.
 * @param  None
 * @retval None
 */
void HardFault_Handler(void) {
    // Disable PWM outputs
    TIM1->BDTR &= ~(TIM_BDTR_MOE);
    /* Go to infinite loop when Hard Fault exception occurs */
    while (1) {
    }
}

/**
 * @brief  This function handles Memory Manage exception.
 * @param  None
 * @retval None
 */
void MemManage_Handler(void) {
    // Disable PWM outputs
    TIM1->BDTR &= ~(TIM_BDTR_MOE);
    /* Go to infinite loop when Memory Manage exception occurs */
    while (1) {
    }
}

/**
 * @brief  This function handles Bus Fault exception.
 * @param  None
 * @retval None
 */
void BusFault_Handler(void) {
    // Disable PWM outputs
    TIM1->BDTR &= ~(TIM_BDTR_MOE);
    /* Go to infinite loop when Bus Fault exception occurs */
    while (1) {
    }
}

/**
 * @brief  This function handles Usage Fault exception.
 * @param  None
 * @retval None
 */
void UsageFault_Handler(void) {
    // Disable PWM outputs
    TIM1->BDTR &= ~(TIM_BDTR_MOE);
    /* Go to infinite loop when Usage Fault exception occurs */
    while (1) {
    }
}

/**
 * @brief  This function handles SVCall exception.
 * @param  None
 * @retval None
 */
void SVC_Handler(void) {
}

/**
 * @brief  This function handles Debug Monitor exception.
 * @param  None
 * @retval None
 */
void DebugMon_Handler(void) {
}

/**
 * @brief  This function handles PendSVC exception.
 * @param  None
 * @retval None
 */
void PendSV_Handler(void) {
}

/**
 * @brief  This function handles SysTick Handler.
 * @param  None
 * @retval None
 */
void SysTick_Handler(void) {
    SYSTICK_IRQHandler();
}

/******************************************************************************/
/*                 STM32F4xx Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f4xx.s).                                               */
/******************************************************************************/

/**
 * Interrupt priorities (lower number = higher priority)
 *
 * TIM1_UP_TIM10_IRQ:		0 (set in pwm.c)
 * TIM8_BRK_TIM12_IRQ:		3 (set in main.c)
 * TIM3_IRQ:				1 (set in hallSensor.c)
 * ADC_IRQ:					2 (set in adc.c)
 * OTG_FS_IRQ:				6 (set in usbd_conf.c)
 * USART3_IRQ:				4 (set in uart.c)
 *
 */

/**
 * @brief  This function handles USB-On-The-Go FS global interrupt request.
 * @param  None
 * @retval None
 */
#ifdef USE_USB_FS
void OTG_FS_IRQHandler(void)
#else
void OTG_HS_IRQHandler(void)
#endif
{
    /*  HAL_PCD_IRQHandler(&hpcd);  */
    USB_IRQ();
}

void TIM1_UP_TIM10_IRQHandler(void) {
    //HAL_TIM_IRQHandler(&hpwm);
    if ((TIM1->SR & TIM_SR_UIF) == TIM_SR_UIF) {
        TIM1->SR = ~(TIM_SR_UIF);
        User_PWMTIM_IRQ();
    }
}

void TIM8_BRK_TIM12_IRQHandler(void) {
    //HAL_TIM_IRQHandler(&hBasicTim);
    if ((TIM12->SR & TIM_SR_UIF) == TIM_SR_UIF) {
        TIM12->SR = ~(TIM_SR_UIF);
        User_BasicTIM_IRQ();
    }

}

void TIM3_IRQHandler(void) {
    //HAL_TIM_IRQHandler(&hHallTim);
    if ((TIM3->SR & TIM_SR_UIF) == TIM_SR_UIF) {
        TIM3->SR = ~(TIM_SR_UIF);
        HallSensor_UpdateCallback();
    }
    if ((TIM3->SR & TIM_SR_CC1IF) == TIM_SR_CC1IF) {
        TIM3->SR = ~(TIM_SR_CC1IF);
        HallSensor_CaptureCallback();
    }
    //deli_hall_capture_isr();
}

void TIM8_UP_TIM13_IRQHandler(void) {
    // PAS1 timer overflow event
    // Reset the timer source
    if ((TIM13->SR & TIM_SR_UIF) == TIM_SR_UIF) {
        TIM13->SR = ~(TIM_SR_UIF);
        throttle_pas_timer_overflow(1);
    }
}

void TIM8_TRG_COM_TIM14_IRQHandler(void) {
    // PAS2 timer overflow event
    // Reset the timer source
    if ((TIM14->SR & TIM_SR_UIF) == TIM_SR_UIF) {
        TIM14->SR = ~(TIM_SR_UIF);
        throttle_pas_timer_overflow(2);
    }
}

void ADC_IRQHandler(void) {
    if (((ADC1->SR) & ADC_SR_JEOC) == ADC_SR_JEOC) {
        ADC1->SR = ~(ADC_SR_JEOC);
        adcConvComplete();
    }
    if (((ADC1->SR) & ADC_SR_OVR) == ADC_SR_OVR) {
        ADC1->SR = ~(ADC_SR_OVR);
    }
}

void USART2_IRQHandler(void) {
    UART_IRQ(SELECT_BMS_UART);
}

void USART3_IRQHandler(void) {
    UART_IRQ(SELECT_HBD_UART);
}

void EXTI0_IRQHandler(void) {
    // Reset the interrupt source
    EXTI->PR |= EXTI_PR_PR0;
    // Call the appropriate PAS process
    throttle_pas_process(2);
}

void EXTI9_5_IRQHandler(void) {
    // Reset the interrupt source
    EXTI->PR |= EXTI_PR_PR5;
    // Call the appropriate PAS process
    throttle_pas_process(1);
}

#ifdef USE_UART

/**
 * @brief  This function handles DMA interrupt request.
 * @param  None
 * @retval None
 */
void USARTx_DMA_TX_IRQHandler(void)
{
    HAL_DMA_IRQHandler(UartHandle.hdmatx);
}

/**
 * @brief  This function handles UART interrupt request.
 * @param  None
 * @retval None
 */
void USARTx_IRQHandler(void)
{
    HAL_UART_IRQHandler(&UartHandle);
}

/**
 * @brief  This function handles TIM interrupt request.
 * @param  None
 * @retval None
 */
void TIMx_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&TimHandle);
}

#endif

/**
 * @brief  This function handles PPP interrupt request.
 * @param  None
 * @retval None
 */
/*void PPP_IRQHandler(void)
 {
 }*/

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
