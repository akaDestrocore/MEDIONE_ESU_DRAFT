#include "main.h"
#include "image.h"
#include "crc.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "stm32f4xx_hal.h"

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
void Error_Handler(void);
void HardFault_Handler(void);

static void boot_to_image(uint32_t addr)
{
    uint32_t vectorAddr = addr + IMAGE_HDR_SIZE;

    uint32_t stackAddr  = *((uint32_t *)(vectorAddr));
    uint32_t resetVector = *((uint32_t *)(vectorAddr + 4U));

    HAL_RCC_DeInit();
    HAL_DeInit();

    /* Remap flash to base address */
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    SYSCFG->MEMRMP = 0x01;

    /* Disable SysTick */
    SysTick->CTRL = 0;

    /* Clear pending SysTick */
    SCB->ICSR |= SCB_ICSR_PENDSTCLR_Msk;

    /* Disable configurable fault handlers */
    SCB->SHCSR &= ~(SCB_SHCSR_USGFAULTENA_Msk |
                    SCB_SHCSR_BUSFAULTENA_Msk  |
                    SCB_SHCSR_MEMFAULTENA_Msk);

    /* Relocate vector table */
    SCB->VTOR = (uint32_t)vectorAddr;

    /* Set stack pointer and jump */
    __set_MSP(stackAddr);
    __set_PSP(stackAddr);

    void (*jump_func)(void) = (void (*)(void))resetVector;
    jump_func();
}

/* STM32F407-Discovery LEDs: PD12=green, PD13=orange, PD14=red, PD15=blue */
static void set_led(uint8_t ledPin, uint8_t state)
{
    uint16_t pin;
    switch (ledPin) {
        case 0: {  
          pin = GPIO_PIN_12; 
          break;
        }
        case 1: { 
          pin = GPIO_PIN_13; 
          break;
        }
        case 2: { 
          pin = GPIO_PIN_14; 
          break;
        }
        case 3: { 
          pin = GPIO_PIN_15; 
          break;
        }
        default: 
          return;
    }

    if (state) {
        HAL_GPIO_WritePin(GPIOD, pin, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(GPIOD, pin, GPIO_PIN_RESET);
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    crc_init();

    const image_hdr_t *header = (const image_hdr_t *)APP_ADDR;

    if (IMAGE_MAGIC_APP == header->image_magic &&
        IMAGE_TYPE_APP  == header->image_type  &&
        CRC_OK == crc_verifyFirmware(APP_ADDR, IMAGE_HDR_SIZE))
    {
        boot_to_image(APP_ADDR);
    } else {
        
        if( CRC_INVALID_SIZE == crc_invalidateFirmware(APP_ADDR)) {
            // Error
        }

        bool ledState = false;
        while (1)
        {
            ledState = !ledState;
            set_led(2, ledState);   /* Red LED */
            HAL_Delay(200);
        }
    }

    while (1) {
        
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM       = 4;
    RCC_OscInitStruct.PLL.PLLN       = 90;
    RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ       = 4;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOD_CLK_ENABLE();

    /* PD12-PD15: LED outputs */
    GPIO_InitStruct.Pin   = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* Start with all LEDs off */
    HAL_GPIO_WritePin(GPIOD,
        GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15,
        GPIO_PIN_RESET);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}

void HardFault_Handler(void) {
    volatile uint32_t cfsr  = SCB->CFSR;
    volatile uint32_t bfar  = SCB->BFAR;
    volatile uint32_t hfsr  = SCB->HFSR;
    (void)cfsr; (void)bfar; (void)hfsr;
    while (1) {

    }
}