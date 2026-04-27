/**
 * ╔═══════════════════════════════════════════════════════════════════════╗
 * ║                       Electrosurgical Unit                            ║
 * ╚═══════════════════════════════════════════════════════════════════════╝
 * 
 * @file           bootloader.c
 * @brief          Bare-metal bootloader designed to fit into 16KB region
 * 
 * @author         destrocore
 * @date           2026
 * 
 * @details
 * Implements system initialization, firmware image validation using CRC32,
 * and safe jump into application image.
 */

#include "main.h"
#include "image.h"
#include "stm32f407xx.h"

/* Private function prototypes -----------------------------------------------*/
static void rcc_init(void);
static void gpio_init(void);
static inline void led_set(uint8_t led, bool on);
static void delay_ms(uint32_t ms);
static uint32_t crc_calculate(uint32_t addr, uint32_t size);
static bool bl_isImageValid(void);
static void deinit_system(void);
static __attribute__((noreturn)) void boot_to_image(void);

/**
  * @brief  Main entry point of bootloader
  * @retval int Never returns in normal operation
  */
int main(void) {
    
    rcc_init();
    gpio_init();

    if (bl_isImageValid()) {
        boot_to_image(); 
    }

    bool ledState = false;
    while (1) {
        ledState = !ledState;
        led_set(2U, ledState);
        delay_ms(200U);
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

/**
  * @brief  Set or reset LED output state
  * @param  led LED index (0..3)
  * @param  on  LED state
  * @retval None
  */
static inline void led_set(uint8_t led, bool on) {

    static const uint16_t PIN[4] = {
        GPIO_BSRR_BS12,  // green
        GPIO_BSRR_BS13,  // orange
        GPIO_BSRR_BS14,  // red
        GPIO_BSRR_BS15,  // blue
    };

    if (led > 3U) { 
        return; 
    }

    if (true == on) {
        GPIOD->BSRR = PIN[led];
    } else {
        GPIOD->BSRR &= ~(PIN[led] << 16U);
    }
}

/**
  * @brief  Blocking delay loop in ms
  * @param  ms Delay duration
  * @retval None
  */
static void delay_ms(uint32_t ms) {

    volatile uint32_t count = ms * 4000UL;

    while (count--) {
        __NOP(); 
    }
}

/**
 * @brief Calculate STM32 hardware CRC32 over a flash region.
 * @param addr  Start address of the data
 * @param size  Number of bytes
 * @return  CRC32 result
 * @note    Data must be 4 byte aligned
 */
static uint32_t crc_calculate(uint32_t addr, uint32_t size) {
    
    // Reset CRC repiph
    CRC->CR = CRC_CR_RESET;

    // Feed whole 32-bit words
    const uint32_t *pAddr = (const uint32_t *)addr;
    // Size/4
    uint32_t words = size >> 2U;
    while (words--) {
        CRC->DR = *pAddr++;
    }

    // Feed remaining bytes as a single word
    uint32_t remaining = size & 3U;
    if (remaining > 0U) {
        const uint8_t *pByte = (const uint8_t *)p;
        uint32_t last = 0U;
        for (uint32_t i = 0U; i < remaining; i++) {
            last |= (uint32_t)pByte[i] << (i * 8U);
        }
        CRC->DR = last;
    }

    return CRC->DR;
}

/**
 * @brief Validate the application image located at APP_ADDR.
 * @return true if the image is valid and safe to boot, false otherwise
 */
static bool bl_isImageValid(void) {
    
    const image_hdr_t *pHdr = (const image_hdr_t *)APP_ADDR;

    if (pHdr->image_magic != IMAGE_MAGIC_APP) {
        return false;
    }

    if (pHdr->image_type != IMAGE_TYPE_APP) {
        return false;
    }

    const uint32_t fw_addr = APP_ADDR + IMAGE_HDR_SIZE;

    if (0U == pHdr->data_size) {
        return false;
    }
    if ((fw_addr + pHdr->data_size) > FLASH_END) {
        return false;
    }
    // fw_addr + data_size must not wrap
    if ((fw_addr + pHdr->data_size) < fw_addr) {
        return false;
    }

    uint32_t calculated = crc_calculate(fw_addr, pHdr->data_size);
    return (calculated == pHdr->crc);
}

/**
 * @brief De-initialize all peripherals
 */
static void deinit_system(void) {

    // AHB1
    RCC->AHB1RSTR |= RCC_AHB1RSTR_CRCRST;
    RCC->AHB1RSTR |= RCC_AHB1RSTR_GPIOARST;
    RCC->AHB1RSTR |= RCC_AHB1RSTR_GPIOFRST;

    RCC->AHB1RSTR &= ~(RCC_AHB1RSTR_CRCRST 
                    | RCC_AHB1RSTR_GPIOARST 
                    | RCC_AHB1RSTR_GPIOFRST);

    // APB1
    RCC->APB1RSTR |= RCC_APB1RSTR_PWRRST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_PWRRST;

    // APB2
    RCC->APB2RSTR |= RCC_APB2RSTR_USART1RST |
                     RCC_APB2RSTR_SYSCFGRST;

    RCC->APB2RSTR &= ~(RCC_APB2RSTR_USART1RST |
                       RCC_APB2RSTR_SYSCFGRST);
    
    // Disbale peripheral buff 
    RCC->AHB1ENR &= ~(RCC_AHB1ENR_CRCEN |
                      RCC_AHB1ENR_GPIOAEN |
                      RCC_AHB1ENR_GPIOFEN);

    RCC->APB1ENR &= ~(RCC_APB1ENR_PWREN);

    RCC->APB2ENR &= ~(RCC_APB2ENR_USART1EN |
                      RCC_APB2ENR_SYSCFGEN);

    // AHB3
    RCC->AHB3ENR &= ~(RCC_AHB3ENR_QSPIEN);

    // Reset all NVICs
    for (uint32_t i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;
}

/**
 * @brief Hand control to the application.
 */
static __attribute__((noreturn)) void boot_to_image(void)
{
    const uint32_t vectorAddr = APP_ADDR + IMAGE_HDR_SIZE;
    const uint32_t appMsp = *(const uint32_t *)(vectorAddr);
    const uint32_t appResetFn = *(const uint32_t *)(vectorAddr + 4U);

    // All LEDs off
    GPIOD->BSRR = (GPIO_BSRR_BS12 | GPIO_BSRR_BS13 
                |GPIO_BSRR_BS14 | GPIO_BSRR_BS15) << 16U;

    // Reset GPIOD to input
    GPIOD->MODER  = 0x00000000U;
    GPIOD->OTYPER = 0x00000000U;
    GPIOD->PUPDR  = 0x00000000U;

    RCC->AHB1ENR &= ~(RCC_AHB1ENR_CRCEN | RCC_AHB1ENR_GPIODEN);

    SCB->VTOR = vectorAddr;

    SysTick->CTRL = 0U;
    SCB->ICSR |= SCB_ICSR_PENDSTCLR_Msk;
    SCB->SHCSR &= ~(SCB_SHCSR_USGFAULTENA_Msk 
                | SCB_SHCSR_BUSFAULTENA_Msk 
                | SCB_SHCSR_MEMFAULTENA_Msk);

    SYSCFG->MEMRMP = 0U;

    __set_MSP(appMsp);
    __set_PSP(appMsp);

    void (*pAppReset)(void) = (void (*)(void))appResetFn;
    pAppReset();

    while (1) {
        // Should never reach here
    }
}

/* --------------------------------------------------------------------------
 * Fault / error handlers
 * -------------------------------------------------------------------------*/

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}

/**
 * @brief HardFault handler
 *
 * Volatile reads prevent the compiler from optimising away the
 * register captures, making them visible in a debugger
 */
void HardFault_Handler(void)
{
    volatile uint32_t cfsr = SCB->CFSR;     // Configurable Fault Status
    volatile uint32_t hfsr = SCB->HFSR;     // HardFault Status
    volatile uint32_t bfar = SCB->BFAR;     // Bus Fault Address
    volatile uint32_t mmar = SCB->MMFAR;    //MemManage Fault Address
    (void)cfsr; (void)hfsr; (void)bfar; (void)mmar;
    while (1) {
        __NOP();
    }
}