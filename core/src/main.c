/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_fsm.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

DAC_HandleTypeDef hdac;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;
TIM_HandleTypeDef htim5;
TIM_HandleTypeDef htim9;
TIM_HandleTypeDef htim13;
TIM_HandleTypeDef htim14;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;

/*
 * DMA handle for USART3 RX (DMA1 Stream1 Channel4).
 * Declared here so that nextion.c can reference it via extern.
 * Must be defined before HAL_UART_MspInit links it to the UART handle.
 */
DMA_HandleTypeDef hdma_usart3_rx;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_DMA_Init(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM9_Init(void);
static void MX_DAC_Init(void);
static void MX_TIM4_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM13_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM14_Init(void);
static void MX_TIM5_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void) {

    /* USER CODE BEGIN 1 */

    /* USER CODE END 1 */

    /* MCU Configuration -------------------------------------------------------*/
    HAL_Init();

    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    SystemClock_Config();

    /* USER CODE BEGIN SysInit */

    /* USER CODE END SysInit */

    MX_GPIO_Init();
    MX_DMA_Init();           // Must come BEFORE MX_USART3_UART_Init
    MX_USART3_UART_Init();
    MX_ADC1_Init();
    MX_TIM9_Init();
    MX_DAC_Init();
    MX_TIM4_Init();
    MX_TIM2_Init();
    MX_TIM13_Init();
    MX_TIM3_Init();
    MX_TIM14_Init();
    MX_TIM5_Init();
    MX_USART1_UART_Init();

    /* USER CODE BEGIN 2 */
    {
        const RFGen_Timers_t timers = {
            .pHtimCut      = &htim4,
            .pHtimCoag     = &htim13,
            .pHtimCoag2    = &htim14,
            .pHtimAudio    = &htim9,
            .pHtimBlend    = &htim2,
            .pHtimPolySlow = &htim3,
            .pHtimPolyTick = &htim5,
        };
        app_fsm_init(&timers, &hadc1, &hdac, &huart3);
    }
    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1) {
        /* USER CODE END WHILE */
        app_fsmProcess();
        /* USER CODE BEGIN 3 */
    }
    /* USER CODE END 3 */
}

/**
  * @brief  System Clock Configuration — 168 MHz via HSE PLL.
  * @retval None
  */
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM       = 8;
    RCC_OscInitStruct.PLL.PLLN       = 336;
    RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ       = 7;
    if (HAL_OK != HAL_RCC_OscConfig(&RCC_OscInitStruct)) {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK  | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV4;

    if (HAL_OK != HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5)) {
        Error_Handler();
    }
}

/**
  * @brief  Enable DMA controller clocks.
  * @note   Must be called before any peripheral that uses DMA.
  * @retval None
  */
static void MX_DMA_Init(void) {
    __HAL_RCC_DMA1_CLK_ENABLE();
}

/**
  * @brief  ADC1 Initialization.
  * @retval None
  */
static void MX_ADC1_Init(void) {
    ADC_ChannelConfTypeDef sConfig = {0};

    hadc1.Instance                   = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV2;
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode          = ENABLE;
    hadc1.Init.ContinuousConvMode    = ENABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion       = 5;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    if (HAL_OK != HAL_ADC_Init(&hadc1)) {
        Error_Handler();
    }

    sConfig.SamplingTime = ADC_SAMPLETIME_56CYCLES;

    sConfig.Channel = ADC_CHANNEL_0; sConfig.Rank = 1;
    if (HAL_OK != HAL_ADC_ConfigChannel(&hadc1, &sConfig)) { Error_Handler(); }

    sConfig.Channel = ADC_CHANNEL_1; sConfig.Rank = 2;
    if (HAL_OK != HAL_ADC_ConfigChannel(&hadc1, &sConfig)) { Error_Handler(); }

    sConfig.Channel = ADC_CHANNEL_2; sConfig.Rank = 3;
    if (HAL_OK != HAL_ADC_ConfigChannel(&hadc1, &sConfig)) { Error_Handler(); }

    sConfig.Channel = ADC_CHANNEL_3; sConfig.Rank = 4;
    if (HAL_OK != HAL_ADC_ConfigChannel(&hadc1, &sConfig)) { Error_Handler(); }

    sConfig.Channel = ADC_CHANNEL_8; sConfig.Rank = 5;
    if (HAL_OK != HAL_ADC_ConfigChannel(&hadc1, &sConfig)) { Error_Handler(); }
}

/**
  * @brief  DAC Initialization.
  * @retval None
  */
static void MX_DAC_Init(void) {
    DAC_ChannelConfTypeDef sConfig = {0};

    hdac.Instance = DAC;
    if (HAL_OK != HAL_DAC_Init(&hdac)) {
        Error_Handler();
    }

    sConfig.DAC_Trigger      = DAC_TRIGGER_NONE;
    sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
    if (HAL_OK != HAL_DAC_ConfigChannel(&hdac, &sConfig, DAC_CHANNEL_1)) {
        Error_Handler();
    }
}

/**
  * @brief  TIM2 PWM Initialization (Blend envelope, 33 kHz / PA5).
  * @retval None
  */
static void MX_TIM2_Init(void) {
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    TIM_OC_InitTypeDef      sConfigOC     = {0};

    htim2.Instance               = TIM2;
    htim2.Init.Prescaler         = 10U - 1U;
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period            = 254U - 1U;
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_OK != HAL_TIM_PWM_Init(&htim2)) { Error_Handler(); }

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_OK != HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig)) { Error_Handler(); }

    sConfigOC.OCMode     = TIM_OCMODE_PWM1;
    sConfigOC.Pulse      = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_OK != HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1)) { Error_Handler(); }

    HAL_TIM_MspPostInit(&htim2);
}

/**
  * @brief  TIM3 PWM Initialization (Polypectomy slow pulse ~1.5 Hz / PC6).
  * @retval None
  */
static void MX_TIM3_Init(void) {
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    TIM_OC_InitTypeDef      sConfigOC     = {0};

    htim3.Instance               = TIM3;
    htim3.Init.Prescaler         = 21000U - 1U;
    htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim3.Init.Period            = 2600U - 1U;
    htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_OK != HAL_TIM_PWM_Init(&htim3)) { Error_Handler(); }

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_OK != HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig)) { Error_Handler(); }

    sConfigOC.OCMode     = TIM_OCMODE_PWM1;
    sConfigOC.Pulse      = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_OK != HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1)) { Error_Handler(); }

    HAL_TIM_MspPostInit(&htim3);
}

/**
  * @brief  TIM4 PWM Initialization (CUT carrier / PD12).
  * @retval None
  */
static void MX_TIM4_Init(void) {
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    TIM_OC_InitTypeDef      sConfigOC     = {0};

    htim4.Instance               = TIM4;
    htim4.Init.Prescaler         = 21U - 1U;
    htim4.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim4.Init.Period            = 8U - 1U;
    htim4.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_OK != HAL_TIM_PWM_Init(&htim4)) { Error_Handler(); }

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_OK != HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig)) { Error_Handler(); }

    sConfigOC.OCMode     = TIM_OCMODE_PWM1;
    sConfigOC.Pulse      = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_OK != HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_1)) { Error_Handler(); }

    HAL_TIM_MspPostInit(&htim4);
}

/**
  * @brief  TIM5 Initialization (Polypectomy cycle tick, 100 Hz).
  * @retval None
  */
static void MX_TIM5_Init(void) {
    TIM_ClockConfigTypeDef  sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig      = {0};

    htim5.Instance               = TIM5;
    htim5.Init.Prescaler         = 24000U - 1U;
    htim5.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim5.Init.Period            = 35U - 1U;
    htim5.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_OK != HAL_TIM_Base_Init(&htim5)) { Error_Handler(); }

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_OK != HAL_TIM_ConfigClockSource(&htim5, &sClockSourceConfig)) { Error_Handler(); }

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_OK != HAL_TIMEx_MasterConfigSynchronization(&htim5, &sMasterConfig)) { Error_Handler(); }
}

/**
  * @brief  TIM9 PWM Initialization (Audio buzzer / PE6).
  * @retval None
  */
static void MX_TIM9_Init(void) {
    TIM_OC_InitTypeDef sConfigOC = {0};

    htim9.Instance               = TIM9;
    htim9.Init.Prescaler         = 210U - 1U;
    htim9.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim9.Init.Period            = 600U - 1U;
    htim9.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim9.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_OK != HAL_TIM_PWM_Init(&htim9)) { Error_Handler(); }

    sConfigOC.OCMode     = TIM_OCMODE_PWM1;
    sConfigOC.Pulse      = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_OK != HAL_TIM_PWM_ConfigChannel(&htim9, &sConfigOC, TIM_CHANNEL_2)) { Error_Handler(); }

    HAL_TIM_MspPostInit(&htim9);
}

/**
  * @brief  TIM13 PWM Initialization (COAG Soft / Bipolar carrier / PA6).
  * @retval None
  */
static void MX_TIM13_Init(void) {
    TIM_OC_InitTypeDef sConfigOC = {0};

    htim13.Instance               = TIM13;
    htim13.Init.Prescaler         = 21U - 1U;
    htim13.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim13.Init.Period            = 10U - 1U;
    htim13.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim13.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_OK != HAL_TIM_Base_Init(&htim13))  { Error_Handler(); }
    if (HAL_OK != HAL_TIM_PWM_Init(&htim13))   { Error_Handler(); }

    sConfigOC.OCMode     = TIM_OCMODE_PWM1;
    sConfigOC.Pulse      = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_OK != HAL_TIM_PWM_ConfigChannel(&htim13, &sConfigOC, TIM_CHANNEL_1)) { Error_Handler(); }

    HAL_TIM_MspPostInit(&htim13);
}

/**
  * @brief  TIM14 PWM Initialization (COAG Contact/Spray/Argon / PA7).
  * @retval None
  */
static void MX_TIM14_Init(void) {
    TIM_OC_InitTypeDef sConfigOC = {0};

    htim14.Instance               = TIM14;
    htim14.Init.Prescaler         = 21U - 1U;
    htim14.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim14.Init.Period            = 100U - 1U;
    htim14.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim14.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_OK != HAL_TIM_Base_Init(&htim14))  { Error_Handler(); }
    if (HAL_OK != HAL_TIM_PWM_Init(&htim14))   { Error_Handler(); }

    sConfigOC.OCMode     = TIM_OCMODE_PWM1;
    sConfigOC.Pulse      = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_OK != HAL_TIM_PWM_ConfigChannel(&htim14, &sConfigOC, TIM_CHANNEL_1)) { Error_Handler(); }

    HAL_TIM_MspPostInit(&htim14);
}

/**
  * @brief  USART1 Initialization (firmware update, 115200 baud).
  * @retval None
  */
static void MX_USART1_UART_Init(void) {
    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_OK != HAL_UART_Init(&huart1)) {
        Error_Handler();
    }
}

/**
  * @brief  USART3 Initialization (Nextion display, 9600 baud).
  * @retval None
  */
static void MX_USART3_UART_Init(void) {
    huart3.Instance          = USART3;
    huart3.Init.BaudRate     = 9600;
    huart3.Init.WordLength   = UART_WORDLENGTH_8B;
    huart3.Init.StopBits     = UART_STOPBITS_1;
    huart3.Init.Parity       = UART_PARITY_NONE;
    huart3.Init.Mode         = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_OK != HAL_UART_Init(&huart3)) {
        Error_Handler();
    }
}

/**
  * @brief  GPIO Initialization.
  * @retval None
  */
static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    // Output — PE
    HAL_GPIO_WritePin(GPIOE,
        GPIO_PIN_2  | GPIO_PIN_3  | GPIO_PIN_7  | GPIO_PIN_8  |
        GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 |
        GPIO_PIN_15 | GPIO_PIN_0  | GPIO_PIN_1,
        GPIO_PIN_RESET);

    // Output — PC
    HAL_GPIO_WritePin(GPIOC,
        GPIO_PIN_13 | GPIO_PIN_2 | GPIO_PIN_4 | GPIO_PIN_5 |
        GPIO_PIN_7  | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12,
        GPIO_PIN_RESET);

    // Output — PB
    HAL_GPIO_WritePin(GPIOB,
        GPIO_PIN_1 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15,
        GPIO_PIN_RESET);

    // Output — PD
    HAL_GPIO_WritePin(GPIOD,
        GPIO_PIN_8  | GPIO_PIN_9  | GPIO_PIN_10 | GPIO_PIN_11 |
        GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15 | GPIO_PIN_0  |
        GPIO_PIN_1  | GPIO_PIN_2  | GPIO_PIN_3  | GPIO_PIN_4  |
        GPIO_PIN_5  | GPIO_PIN_6  | GPIO_PIN_7,
        GPIO_PIN_RESET);

    // Output — PA
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8 | GPIO_PIN_11 | GPIO_PIN_12, GPIO_PIN_RESET);

    // PE outputs
    GPIO_InitStruct.Pin =
        GPIO_PIN_2  | GPIO_PIN_3  | GPIO_PIN_7  | GPIO_PIN_8  |
        GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 |
        GPIO_PIN_15 | GPIO_PIN_0  | GPIO_PIN_1;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    // PC outputs
    GPIO_InitStruct.Pin =
        GPIO_PIN_13 | GPIO_PIN_2 | GPIO_PIN_4 | GPIO_PIN_5 |
        GPIO_PIN_7  | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    // PC inputs (footswitch COAG Mono1 + REM)
    GPIO_InitStruct.Pin  = GPIO_PIN_1 | GPIO_PIN_8;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    // PB outputs
    GPIO_InitStruct.Pin  = GPIO_PIN_1 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // PE inputs (footswitch Mono2)
    GPIO_InitStruct.Pin  = GPIO_PIN_9 | GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    // PD outputs
    GPIO_InitStruct.Pin =
        GPIO_PIN_8  | GPIO_PIN_9  | GPIO_PIN_10 | GPIO_PIN_11 |
        GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15 | GPIO_PIN_0  |
        GPIO_PIN_1  | GPIO_PIN_2  | GPIO_PIN_3  | GPIO_PIN_4  |
        GPIO_PIN_5  | GPIO_PIN_6  | GPIO_PIN_7;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    // PA outputs
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_11 | GPIO_PIN_12;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // PB inputs (handswitch + bipolar auto)
    GPIO_InitStruct.Pin  = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_7 | GPIO_PIN_8;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/* USER CODE BEGIN 4 */

/**
  * @brief  Timer period elapsed callback — dispatches to the FSM.
  * @param  pHtim  Timer handle that triggered the callback.
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *pHtim) {
    if (pHtim->Instance == TIM5) {
        app_fsmPolyTick();
    }

    if (pHtim->Instance == TIM2) {
        app_fsmBlendTick();
    }
}

/* USER CODE END 4 */

/**
  * @brief  Error handler — halts execution with interrupts disabled.
  * @retval None
  */
void Error_Handler(void) {
    __disable_irq();
    while (1) {
        // Spin — attach debugger and inspect call stack
    }
}

#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the source file and line of a failed assert_param().
  * @param  pFile  Source file name.
  * @param  line   Line number.
  * @retval None
  */
void assert_failed(uint8_t *pFile, uint32_t line) {
    (void)pFile;
    (void)line;
}
#endif /* USE_FULL_ASSERT */