/**
 * ╔═══════════════════════════════════════════════════════════════════════╗
 * ║                       Electrosurgical Unit                            ║
 * ╚═══════════════════════════════════════════════════════════════════════╝
 * 
 * @file           updater.c
 * @brief          TODO
 * 
 * @author         destrocore
 * @date           2026
 * 
 * @details
 * TODO
 */

#include "main.h"
#include "image.h"
#include "stm32f407xx.h"

/* Private function prototypes -----------------------------------------------*/
static void rcc_init(void);
static void gpio_init(void);
void Error_Handler(void);
void HardFault_Handler(void);

/**
  * @brief  Main entry point of bootloader
  * @retval int Never returns in normal operation
  */
int main(void) {
    
    rcc_init();
    gpio_init();

    while (1) {

    }
}

/**
  * @brief  Initialize RCC clock system to HSI
  * @retval None
  */
static void rcc_init(void) {
    
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY)) {
        // Wait for HSI to be ready
    }

    // Switch SYSCLK to HSI
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_HSI;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI) {
        // Wait for HSI bit to set
    }

    // Enable peripheral clocks
    RCC->AHB1ENR |= RCC_AHB1ENR_CRCEN | RCC_AHB1ENR_GPIODEN;
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    (void)RCC->AHB1ENR;
    (void)RCC->APB2ENR;
}


/**
  * @brief  Initialize GPIO
  * @retval None
  */
static void gpio_init(void) {
    
    // Output mode for PD12-PD15
    GPIOD->MODER &= ~(0xFFUL << 24U);
    GPIOD->MODER |=  GPIO_MODER_MODER12_0 | GPIO_MODER_MODER13_0 
                    | GPIO_MODER_MODER14_0 | GPIO_MODER_MODER15_0;

    // Push-pull
    GPIOD->OTYPER &= ~(GPIO_OTYPER_OT12 | GPIO_OTYPER_OT13 
                    | GPIO_OTYPER_OT14 | GPIO_OTYPER_OT15);

    // Low speed
    GPIOD->OSPEEDR &= ~(0xFFUL << 24U);
    GPIOD->PUPDR   &= ~(0xFFUL << 24U);

    // All LEDs off
    GPIOD->BSRR = (GPIO_BSRR_BS12 | GPIO_BSRR_BS13 
                | GPIO_BSRR_BS14 | GPIO_BSRR_BS15) << 16U;
}

// TODO: Add new functions

/* --------------------------------------------------------------------------
 * Fault / error handlers
 * -------------------------------------------------------------------------*/

/**
 * @brief Generic error handler
 * @note This function is called by the HAL library in case of an error.
 */
void Error_Handler(void) {

    __disable_irq();
    while (1) {
        __NOP();
    }
}

/**
 * @brief HardFault handler
 * @note Volatile reads prevent the compiler from optimising away the
 * register captures, making them visible in a debugger
 */
void HardFault_Handler(void) {

    volatile uint32_t cfsr = SCB->CFSR;     // Configurable Fault Status
    volatile uint32_t hfsr = SCB->HFSR;     // HardFault Status
    volatile uint32_t bfar = SCB->BFAR;     // Bus Fault Address
    volatile uint32_t mmar = SCB->MMFAR;    // MemManage Fault Address
    (void)cfsr; (void)hfsr; (void)bfar; (void)mmar;
    while (1) {
        __NOP();
    }
}