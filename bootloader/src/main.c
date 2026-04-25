#include "main.h"
#include "image.h"
#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_gpio.h"

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
void Error_Handler(void);

/**
  * @brief        The application entry point.
  * @param addr:  Image header address
  * @retval None
  */
static void boot_to_image(uint32_t addr) {
  uint32_t vectorAddr = addr + IMAGE_HDR_SIZE;

  // get SP and reset vector
  uint32_t stackAddr = *((uint32_t*)(vectorAddr));
  uint32_t resetVector = *((uint32_t*)(vectorAddr + 4U));

  HAL_RCC_DeInit();
  HAL_DeInit();

  SYSCFG->MEMRMP = 0x01;

  // Disable SysTick
  SysTick->CTRL = 0;

  // Clear PendSV
  SCB->ICSR |= SCB_ICSR_PENDSTCLR_Msk ;

  // Disable fault handlers
  SCB->SHCSR &= ~(  SCB_SHCSR_USGFAULTENA_Msk | \
                    SCB_SHCSR_BUSFAULTENA_Msk | \
                    SCB_SHCSR_MEMFAULTENA_Msk ) ;

  // Move vector table to the new address
  SCB->VTOR = (uint32_t)vectorAddr;

  // Set main SP
  __set_MSP(stackAddr);
	__set_PSP(stackAddr);

  void (*jump_func)(void) = (void (*)(void))resetVector;
  jump_func();
}

/**
  * @brief Set LED state
  * @param ledPin LED pin number (0-3)
  * @param state 1 to turn on, 0 to turn off
  */
static void set_led(uint8_t ledPin, uint8_t state) {
    uint32_t pin;
    switch (ledPin) {
        case 0: {
            pin = LL_GPIO_PIN_12; 
            break;
        }
        case 1: {
            pin = LL_GPIO_PIN_13; 
            break;
        }
        case 2: {
            pin = LL_GPIO_PIN_14; 
            break;
        }
        case 3: {
            pin = LL_GPIO_PIN_15; 
            break;
        }
        default: {
            return;
        }
    }
    
    if (state) {
        LL_GPIO_SetOutputPin(GPIOD, pin);
    } else {
        LL_GPIO_ResetOutputPin(GPIOD, pin);
    }
}

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();

  image_hdr_t header;
  memcpy(&header, (void*)APP_ADDR, sizeof(image_hdr_t));

  if (header.image_magic == IMAGE_MAGIC_APP && header.image_type == IMAGE_TYPE_APP && 0 != crc_verifyFirmware(APP_ADDR, IMAGE_HDR_SIZE)) {
    boot_to_image(APP_ADDR);
  } else {
    // Invalidate destination
    crc_invalidateFirmware(APP_ADDR);

    bool ledState = 1;

    while(1) {

      ledState = !ledState;
      set_led(2, ledState);  // Red LED
      HAL_Delay(200);
    }
  }

  while (1)
  {
    // Do nothing since we won't reach here
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 90;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{

  /* GPIO Ports Clock Enable */
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOH);
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);

}


/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
 __disable_irq();
  while (1)
  {
  }
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
