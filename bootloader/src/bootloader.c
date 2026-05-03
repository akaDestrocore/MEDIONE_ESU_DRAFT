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

/* Private function prototypes -----------------------------------------------*/
static void rcc_init(void);
static void gpio_init(void);
static inline void led_set(uint8_t led, bool on);
static void delay_ms(uint32_t ms);
static uint32_t crc_calculate(uint32_t addr, uint32_t size);
static bool bl_isImageValid(uint32_t baseAddr, uint32_t expectedMagic);
static bl_bootTarget_e bl_getBootTarget(void);
static void deinit_system(void);
static __attribute__((noreturn)) void boot_to_image(uint32_t vectorAddr);
void Error_Handler(void);
void HardFault_Handler(void);


/* Private type deffinitions --------------------------------------------------*/
typedef enum {
    bl_bootTarget_NONE      = 0,
    bl_bootTarget_UPDATER   = 1,
    bl_bootTarget_APP       = 2
} bl_bootTarget_e;

/**
  * @brief  Main entry point of bootloader
  * @retval int Never returns in normal operation
  */
int main(void) {

    rcc_init();
    gpio_init();

    const bl_bootTarget_e target = bl_getBootTarget();

    switch (target) {
        case bl_bootTarget_APP: {
            deinit_system();
            boot_to_image(APP_ADDR + IMAGE_HDR_SIZE);
            break;
        }
        
        case bl_bootTarget_UPDATER: {
            deinit_system();
            boot_to_image(UPDATER_ADDR + IMAGE_HDR_SIZE);
            break;
        }

        default: {
            break;
        }
    }

    // Both images invalid — blink red LED (index 2) forever
    bool ledState = false;
    while (1U) {
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
    while (0U == (RCC->CR & RCC_CR_HSIRDY)) {
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
        GPIOD->BSRR = PIN[led] << 16U;
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
 * @retval  CRC32 result
 * @note    Data must be 4 byte aligned
 */
static uint32_t crc_calculate(uint32_t addr, uint32_t size) {

    CRC->CR = CRC_CR_RESET;

    const uint32_t *pWord = (const uint32_t *)addr;
    uint32_t words = size >> 2U;

    while (words-- > 0U) {
        CRC->DR = *pWord++;
    }

    const uint32_t remaining = size & 3U;
    if (remaining > 0U) {
        const uint8_t *pByte = (const uint8_t *)pWord;

        uint32_t last = 0U;
        for (uint32_t i = 0U; i < remaining; i++) {
            last |= (uint32_t)pByte[i] << (i * 8U);
        }
        CRC->DR = last;
    }

    return CRC->DR;
}

/**
 * @brief Validate one firmware image.
 * @param baseAddr Flash address of the image_hdr_t.
 * @param expectedMagic Magic constant expected in image_hdr_t.image_magic.
 * @retval true if the image header and CRC are valid.
 */
static bool bl_isImageValid(uint32_t baseAddr, uint32_t expectedMagic) {

    const image_hdr_t *pHdr = (const image_hdr_t *)baseAddr;

    if (pHdr->image_magic != expectedMagic) {
        return false;
    }

    if (0U == pHdr->data_size) {
        return false;
    }

    const uint32_t fwAddr = baseAddr + IMAGE_HDR_SIZE;

    if ((fwAddr + pHdr->data_size) > FLASH_END) {
        return false;
    }

    if ((fwAddr + pHdr->data_size) < fwAddr) {
        return false;
    }

    const uint32_t calculated = crc_calculate(fwAddr, pHdr->data_size);
    return (calculated == pHdr->crc);
}


/**
 * @brief Determine which component to boot.
 * @retval bl_bootTarget_e target to boot, or Bl_BootTarget_NONE if both fail.
 */
static bl_bootTarget_e bl_getBootTarget(void) {

    if (true == bl_isImageValid(APP_ADDR, IMAGE_MAGIC_APP)) {
        return bl_bootTarget_APP;
    }

    if (true == bl_isImageValid(UPDATER_ADDR, IMAGE_MAGIC_UPDATER)) {
        return bl_bootTarget_UPDATER;
    }

    return bl_bootTarget_NONE;
}


/**
 * @brief De-initialise peripherals used by the bootloader before handoff.
 * @retval None
 */
static void deinit_system(void) {
    // Reset used AHB1 peripherals
    RCC->AHB1RSTR |= RCC_AHB1RSTR_CRCRST | RCC_AHB1RSTR_GPIOARST
                   | RCC_AHB1RSTR_GPIODRST;
    RCC->AHB1RSTR &= ~(RCC_AHB1RSTR_CRCRST | RCC_AHB1RSTR_GPIOARST
                     | RCC_AHB1RSTR_GPIODRST);

    // Reset used APB peripherals
    RCC->APB1RSTR |= RCC_APB1RSTR_PWRRST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_PWRRST;

    RCC->APB2RSTR |= RCC_APB2RSTR_SYSCFGRST;
    RCC->APB2RSTR &= ~RCC_APB2RSTR_SYSCFGRST;

    // Disable peripheral clocks
    RCC->AHB1ENR &= ~(RCC_AHB1ENR_CRCEN | RCC_AHB1ENR_GPIOAEN
                    | RCC_AHB1ENR_GPIODEN);
    RCC->APB1ENR &= ~RCC_APB1ENR_PWREN;
    RCC->APB2ENR &= ~RCC_APB2ENR_SYSCFGEN;

    // Clear all NVIC enables and pending bits
    for (uint32_t i = 0U; i < 8U; i++) {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }

    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL  = 0U;
}

/**
 * @brief Transfer control to the image whose vector table starts at vectorAddr.
 * @param vectorAddr Address of the target image's vector table.
 * @retval Never returns.
 */
static __attribute__((noreturn)) void boot_to_image(uint32_t vectorAddr) {

    const uint32_t appMsp = *(const uint32_t *)(vectorAddr);
    const uint32_t appResetFn = *(const uint32_t *)(vectorAddr + 4U);

    // LEDs off
    GPIOD->BSRR = (GPIO_BSRR_BS12 | GPIO_BSRR_BS13 | GPIO_BSRR_BS14 
                    | GPIO_BSRR_BS15) << 16U;

    // Reset GPIOD to input-only state
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

    void (*pReset)(void) = (void (*)(void))appResetFn;
    pReset();

    while (1) {
        __NOP();
    }
}

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