/**
  ******************************************************************************
  * @file         stm32f4xx_hal_msp.c
  * @brief        This file provides code for the MSP Initialization
  *               and de-Initialization codes.
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

/* Includes ------------------------------------------------------------------*/
#include "main.h"

// hdma_usart3_rx is defined in main.c and owned by nextion.c at runtime.
// Declared here so HAL_UART_MspInit can link it to the UART handle.
extern DMA_HandleTypeDef hdma_usart3_rx;

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *pHtim);
                                                                                                                        /**
  * Initializes the Global MSP.
  */
void HAL_MspInit(void) {

  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_RCC_PWR_CLK_ENABLE();

  HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_0);
  
}

/**
  * @brief ADC MSP Initialization
  * @param pHadc: ADC handle pointer
  * @retval None
  */
void HAL_ADC_MspInit(ADC_HandleTypeDef *pHadc) {

  GPIO_InitTypeDef gpioInitStruct = {0};
  if(ADC1 == pHadc->Instance) {
    // Peripheral clock enable
    __HAL_RCC_ADC1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**ADC1 GPIO Configuration
    PA0-WKUP     ------> ADC1_IN0
    PA1     ------> ADC1_IN1
    PA2     ------> ADC1_IN2
    PA3     ------> ADC1_IN3
    PB0     ------> ADC1_IN8
    */
    gpioInitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
    gpioInitStruct.Mode = GPIO_MODE_ANALOG;
    gpioInitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpioInitStruct);

    gpioInitStruct.Pin = GPIO_PIN_0;
    gpioInitStruct.Mode = GPIO_MODE_ANALOG;
    gpioInitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &gpioInitStruct);
  }
}

/**
  * @brief ADC MSP De-Initialization
  * @param pHadc: ADC handle pointer
  * @retval None
  */
void HAL_ADC_MspDeInit(ADC_HandleTypeDef* pHadc) {
  
  if(ADC1 == pHadc->Instance) {
    // Peripheral clock disable
    __HAL_RCC_ADC1_CLK_DISABLE();

    /**ADC1 GPIO Configuration
    PA0-WKUP     ------> ADC1_IN0
    PA1     ------> ADC1_IN1
    PA2     ------> ADC1_IN2
    PA3     ------> ADC1_IN3
    PB0     ------> ADC1_IN8
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);

    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_0);
  }
}

/**
  * @brief DAC MSP Initialization
  * @param pHdac: DAC handle pointer
  * @retval None
  */
void HAL_DAC_MspInit(DAC_HandleTypeDef* pHdac) {
  
  GPIO_InitTypeDef gpioInitStruct = {0};
  if(DAC == pHdac->Instance) {
    // Peripheral clock enable
    __HAL_RCC_DAC_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**DAC GPIO Configuration
    PA4     ------> DAC_OUT1
    */
    gpioInitStruct.Pin = GPIO_PIN_4;
    gpioInitStruct.Mode = GPIO_MODE_ANALOG;
    gpioInitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpioInitStruct);
  }
}

/**
  * @brief DAC MSP De-Initialization
  * @param pHdac: DAC handle pointer
  * @retval None
  */
void HAL_DAC_MspDeInit(DAC_HandleTypeDef* pHdac) {
  
  if(DAC == pHdac->Instance) {
    // Peripheral clock disable
    __HAL_RCC_DAC_CLK_DISABLE();

    /**DAC GPIO Configuration
    PA4     ------> DAC_OUT1
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_4);
  }
}

/**
  * @brief TIM_PWM MSP Initialization
  * @param pHtimPwm: TIM_PWM handle pointer
  * @retval None
  */
void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef* pHtimPwm) {
  if(TIM2 == pHtimPwm->Instance) {
    // Peripheral clock enable
    __HAL_RCC_TIM2_CLK_ENABLE();
    // TIM2 interrupt Init
    HAL_NVIC_SetPriority(TIM2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
  }
  else if(TIM3 == pHtimPwm->Instance) {
    // Peripheral clock enable
    __HAL_RCC_TIM3_CLK_ENABLE();
  }
  else if(TIM4 == pHtimPwm->Instance) {
    // Peripheral clock enable
    __HAL_RCC_TIM4_CLK_ENABLE();
  }
  else if(pHtimPwm->Instance==TIM9)
  {
    // Peripheral clock enable
    __HAL_RCC_TIM9_CLK_ENABLE();
    // TIM9 interrupt Init
    HAL_NVIC_SetPriority(TIM1_BRK_TIM9_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(TIM1_BRK_TIM9_IRQn);
  }
}

/**
  * @brief TIM_Base MSP Initialization
  * @param pHtimBase: TIM_Base handle pointer
  * @retval None
  */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* pHtimBase) {
  
  if(TIM5 == pHtimBase->Instance) {
    // Peripheral clock enable
    __HAL_RCC_TIM5_CLK_ENABLE();
    // TIM5 interrupt Init
    HAL_NVIC_SetPriority(TIM5_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(TIM5_IRQn);
  }
  else if(TIM13 == pHtimBase->Instance) {
    // Peripheral clock enable
    __HAL_RCC_TIM13_CLK_ENABLE();
  }
  else if(TIM14 == pHtimBase->Instance) {
    // Peripheral clock enable
    __HAL_RCC_TIM14_CLK_ENABLE();
  }
}

/**
  * @brief  TIM MspPostInit — configures GPIO after PWM channels are set up.
  * @param  pHtim TIM handle pointer.
  * @retval None
  */
 void HAL_TIM_MspPostInit(TIM_HandleTypeDef* pHtim) {
  GPIO_InitTypeDef gpioInitStruct = {0};
  if(TIM2 == pHtim->Instance) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**TIM2 GPIO Configuration
    PA5     ------> TIM2_CH1
    */
    gpioInitStruct.Pin = GPIO_PIN_5;
    gpioInitStruct.Mode = GPIO_MODE_AF_PP;
    gpioInitStruct.Pull = GPIO_NOPULL;
    gpioInitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    gpioInitStruct.Alternate = GPIO_AF1_TIM2;
    HAL_GPIO_Init(GPIOA, &gpioInitStruct);

  }
  else if(TIM3 == pHtim->Instance) {

    __HAL_RCC_GPIOC_CLK_ENABLE();
    /**TIM3 GPIO Configuration
    PC6     ------> TIM3_CH1
    */
    gpioInitStruct.Pin = GPIO_PIN_6;
    gpioInitStruct.Mode = GPIO_MODE_AF_PP;
    gpioInitStruct.Pull = GPIO_NOPULL;
    gpioInitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    gpioInitStruct.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOC, &gpioInitStruct);
  }
  else if(TIM4 == pHtim->Instance) {

    __HAL_RCC_GPIOD_CLK_ENABLE();
    /**TIM4 GPIO Configuration
    PD12     ------> TIM4_CH1
    */
    gpioInitStruct.Pin = GPIO_PIN_12;
    gpioInitStruct.Mode = GPIO_MODE_AF_PP;
    gpioInitStruct.Pull = GPIO_NOPULL;
    gpioInitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    gpioInitStruct.Alternate = GPIO_AF2_TIM4;
    HAL_GPIO_Init(GPIOD, &gpioInitStruct);
  }
  else if(TIM9 == pHtim->Instance) {
    __HAL_RCC_GPIOE_CLK_ENABLE();
    /**TIM9 GPIO Configuration
    PE6     ------> TIM9_CH2
    */
    gpioInitStruct.Pin = GPIO_PIN_6;
    gpioInitStruct.Mode = GPIO_MODE_AF_PP;
    gpioInitStruct.Pull = GPIO_NOPULL;
    gpioInitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    gpioInitStruct.Alternate = GPIO_AF3_TIM9;
    HAL_GPIO_Init(GPIOE, &gpioInitStruct);
  }
  else if(TIM13 == pHtim->Instance) {

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**TIM13 GPIO Configuration
    PA6     ------> TIM13_CH1
    */
    gpioInitStruct.Pin = GPIO_PIN_6;
    gpioInitStruct.Mode = GPIO_MODE_AF_PP;
    gpioInitStruct.Pull = GPIO_NOPULL;
    gpioInitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    gpioInitStruct.Alternate = GPIO_AF9_TIM13;
    HAL_GPIO_Init(GPIOA, &gpioInitStruct);
  }
  else if(TIM14 == pHtim->Instance) {

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**TIM14 GPIO Configuration
    PA7     ------> TIM14_CH1
    */
    gpioInitStruct.Pin = GPIO_PIN_7;
    gpioInitStruct.Mode = GPIO_MODE_AF_PP;
    gpioInitStruct.Pull = GPIO_NOPULL;
    gpioInitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    gpioInitStruct.Alternate = GPIO_AF9_TIM14;
    HAL_GPIO_Init(GPIOA, &gpioInitStruct);
  }

}

/**
  * @brief  TIM PWM MSP De-Initialization.
  * @param  pHtimPwm TIM_PWM handle pointer.
  * @retval None
  */
void HAL_TIM_PWM_MspDeInit(TIM_HandleTypeDef *pHtimPwm) {
    if (TIM2 == pHtimPwm->Instance) {

        __HAL_RCC_TIM2_CLK_DISABLE();
        HAL_NVIC_DisableIRQ(TIM2_IRQn);
 
    } 
    else if (TIM3 == pHtimPwm->Instance) {
        __HAL_RCC_TIM3_CLK_DISABLE();
    } 
    else if (TIM4 == pHtimPwm->Instance) {
        __HAL_RCC_TIM4_CLK_DISABLE();
    } 
    else if (TIM9 == pHtimPwm->Instance) {
        __HAL_RCC_TIM9_CLK_DISABLE();
        HAL_NVIC_DisableIRQ(TIM1_BRK_TIM9_IRQn);
    } 
    else {
        // No other PWM timers require de-init
    }
}


/**
  * @brief  TIM Base MSP De-Initialization.
  * @param  pHtimBase TIM_Base handle pointer.
  * @retval None
  */
void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef *pHtimBase) {
    if (TIM5 == pHtimBase->Instance) {
        __HAL_RCC_TIM5_CLK_DISABLE();
        HAL_NVIC_DisableIRQ(TIM5_IRQn);
    } 
    else if (TIM13 == pHtimBase->Instance) {
        __HAL_RCC_TIM13_CLK_DISABLE();
    } 
    else if (TIM14 == pHtimBase->Instance) {
        __HAL_RCC_TIM14_CLK_DISABLE();
    } 
    else {
        // No other base timers require de-init
    }
}


/**
  * @brief  UART MSP Initialization.
  *         Configures GPIO, DMA (USART3 RX only), and NVIC.
  * @param  pHuart UART handle pointer.
  * @retval None
  */
void HAL_UART_MspInit(UART_HandleTypeDef *pHuart) {
    GPIO_InitTypeDef gpioInitStruct = {0};
 
    if (USART1 == pHuart->Instance) {
 
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        // PA9 → USART1_TX, PA10 → USART1_RX
        gpioInitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_10;
        gpioInitStruct.Mode = GPIO_MODE_AF_PP;
        gpioInitStruct.Pull = GPIO_NOPULL;
        gpioInitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        gpioInitStruct.Alternate = GPIO_AF7_USART1;
        HAL_GPIO_Init(GPIOA, &gpioInitStruct);
 
    } 
    else if (USART3 == pHuart->Instance) {
 
        __HAL_RCC_USART3_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
 
        // PB10 → USART3_TX, PB11 → USART3_RX
        gpioInitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_11;
        gpioInitStruct.Mode = GPIO_MODE_AF_PP;
        gpioInitStruct.Pull = GPIO_NOPULL;
        gpioInitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        gpioInitStruct.Alternate = GPIO_AF7_USART3;
        HAL_GPIO_Init(GPIOB, &gpioInitStruct);
/* --------------------------------------------------------------------------
 * DMA1 Stream1 Channel4 — USART3_RX, circular, byte-wide
 * -------------------------------------------------------------------------*/
        hdma_usart3_rx.Instance = DMA1_Stream1;
        hdma_usart3_rx.Init.Channel = DMA_CHANNEL_4;
        hdma_usart3_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
        hdma_usart3_rx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_usart3_rx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_usart3_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart3_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_usart3_rx.Init.Mode = DMA_CIRCULAR;
        hdma_usart3_rx.Init.Priority = DMA_PRIORITY_LOW;
        hdma_usart3_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        (void)HAL_DMA_Init(&hdma_usart3_rx);
 
        // Link DMA handle to UART RX path
        __HAL_LINKDMA(pHuart, hdmarx, hdma_usart3_rx);
 
        // DMA stream IRQ — same priority as USART3
        HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);
 
        // USART3 IRQ — needed for IDLE line detection
        HAL_NVIC_SetPriority(USART3_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(USART3_IRQn);
    } 
    else {
        // No other UARTs require MSP init
    }
}

/**
  * @brief  UART MSP De-Initialization.
  * @param  pHuart UART handle pointer.
  * @retval None
  */
void HAL_UART_MspDeInit(UART_HandleTypeDef *pHuart) {
    if (USART1 == pHuart->Instance) {

        __HAL_RCC_USART1_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9 | GPIO_PIN_10);
    } 
    else if (USART3 == pHuart->Instance) {
 
        __HAL_RCC_USART3_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_10 | GPIO_PIN_11);
 
        (void)HAL_DMA_DeInit(pHuart->hdmarx);
 
        HAL_NVIC_DisableIRQ(DMA1_Stream1_IRQn);
        HAL_NVIC_DisableIRQ(USART3_IRQn);

    } 
    else {
        // No other UARTs require MSP de-init
    }
}