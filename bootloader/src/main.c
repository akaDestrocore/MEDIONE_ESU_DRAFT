#include "main.h"
#include "image.h"

static void rcc_init(void)
{
    // Ensure HSI oscillator is on
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY)) {
        // Wait for HSI to be ready
    }

    // Switch SYSCLK to HSI if not already
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_HSI;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI) {}

    // Enable peripheral clocks
    RCC->AHB1ENR |= RCC_AHB1ENR_CRCEN
                 |  RCC_AHB1ENR_GPIODEN;
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    (void)RCC->AHB1ENR;
    (void)RCC->APB2ENR;
}

static void gpio_init(void)
{
    // Output mode for PD12-PD15
    GPIOD->MODER &= ~(0xFFUL << 24U);
    GPIOD->MODER |=  (0x55UL << 24U);

    // Push-pull
    GPIOD->OTYPER &= ~((1UL << 12U) | (1UL << 13U) |
                       (1UL << 14U) | (1UL << 15U));

    // Low speed
    GPIOD->OSPEEDR &= ~(0xFFUL << 24U);
    GPIOD->PUPDR   &= ~(0xFFUL << 24U);

    // All LEDs off initially
    GPIOD->BSRR = ((1UL << 12U) | (1UL << 13U) |
                   (1UL << 14U) | (1UL << 15U)) << 16U;
}

/**
 * @brief Set or clear a single LED (0=green … 3=blue).
 */
static inline void bl_led_set(uint8_t led, bool on)
{
    static const uint16_t PIN[4] = {
        (1U << 12U),  /* LED0 – green  */
        (1U << 13U),  /* LED1 – orange */
        (1U << 14U),  /* LED2 – red    */
        (1U << 15U),  /* LED3 – blue   */
    };

    if (led > 3U) { return; }

    if (on) {
        GPIOD->BSRR = PIN[led];
    } else {
        GPIOD->BSRR = (uint32_t)PIN[led] << 16U;
    }
}

static void bl_delay_ms(uint32_t ms)
{
    volatile uint32_t count = ms * 4000UL;
    while (count--) { __NOP(); }
}

/**
 * @brief Calculate STM32 hardware CRC32 over a flash region.
 * @param addr  Start address of the data (must be 4-byte aligned).
 * @param size  Number of bytes.
 * @return      CRC32 result.
 */
static uint32_t bl_crc_calculate(uint32_t addr, uint32_t size)
{
    // Reset CRC peripheral
    CRC->CR = CRC_CR_RESET;

    // Feed whole 32-bit words
    const uint32_t *p     = (const uint32_t *)addr;
    uint32_t        words = size >> 2U;    /* size / 4 */
    while (words--) {
        CRC->DR = *p++;
    }

    // Feed remaining bytes as a single word
    uint32_t remaining = size & 3U;
    if (remaining > 0U) {
        const uint8_t *bp   = (const uint8_t *)p;
        uint32_t       last = 0U;
        for (uint32_t i = 0U; i < remaining; i++) {
            last |= (uint32_t)bp[i] << (i * 8U);
        }
        CRC->DR = last;
    }

    return CRC->DR;
}

/**
 * @brief Validate the application image located at BL_APP_ADDR.
 * @return true if the image is valid and safe to boot.
 */
static bool bl_isImageValid(void)
{
    const image_hdr_t *hdr = (const image_hdr_t *)APP_ADDR;

    /* 1. Magic */
    if (hdr->image_magic != IMAGE_MAGIC_APP) {
        return false;
    }

    /* 2. Type */
    if (hdr->image_type != IMAGE_TYPE_APP) {
        return false;
    }

    const uint32_t fw_addr = APP_ADDR + IMAGE_HDR_SIZE;

    if (hdr->data_size == 0U) {
        return false;
    }
    if ((fw_addr + hdr->data_size) > FLASH_END) {
        return false;
    }
    /* Overflow guard: fw_addr + data_size must not wrap */
    if ((fw_addr + hdr->data_size) < fw_addr) {
        return false;
    }

    /* 4. CRC */
    uint32_t calculated = bl_crc_calculate(fw_addr, hdr->data_size);
    return (calculated == hdr->crc);
}

/**
 * @brief Hand control to the application.
 */
static __attribute__((noreturn)) void boot_to_image(void)
{
    const uint32_t vector_addr  = APP_ADDR + IMAGE_HDR_SIZE;
    const uint32_t app_msp      = *(const uint32_t *)(vector_addr);
    const uint32_t app_reset_fn = *(const uint32_t *)(vector_addr + 4U);

    // All LEDs off
    GPIOD->BSRR = ((1UL << 12U) | (1UL << 13U) |
                   (1UL << 14U) | (1UL << 15U)) << 16U;

    // Reset GPIOD to input
    GPIOD->MODER  = 0x00000000U;
    GPIOD->OTYPER = 0x00000000U;
    GPIOD->PUPDR  = 0x00000000U;

    RCC->AHB1ENR &= ~(RCC_AHB1ENR_CRCEN | RCC_AHB1ENR_GPIODEN);

    SCB->VTOR = vector_addr;

    SysTick->CTRL = 0U;
    SCB->ICSR |= SCB_ICSR_PENDSTCLR_Msk;
    SCB->SHCSR &= ~(SCB_SHCSR_USGFAULTENA_Msk |
                    SCB_SHCSR_BUSFAULTENA_Msk  |
                    SCB_SHCSR_MEMFAULTENA_Msk);

    SYSCFG->MEMRMP = 0x00U;

    __set_MSP(app_msp);
    __set_PSP(app_msp);

    void (*app_reset)(void) = (void (*)(void))app_reset_fn;
    app_reset();

    while (1) {
        // Should never reach here
    }
}

int main(void)
{
    rcc_init();
    gpio_init();

    if (bl_isImageValid()) {
        boot_to_image(); 
    }

    bool led_state = false;
    while (1) {
        led_state = !led_state;
        bl_led_set(2U, led_state);
        bl_delay_ms(200U);
    }
}

/* ================================================================== */
/* Fault / error handlers                                              */
/* ================================================================== */

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}

/**
 * @brief HardFault handler with preserved fault status registers.
 *
 * Volatile reads prevent the compiler from optimising away the
 * register captures, making them visible in a debugger.
 */
void HardFault_Handler(void)
{
    volatile uint32_t cfsr = SCB->CFSR;  /* Configurable Fault Status  */
    volatile uint32_t hfsr = SCB->HFSR; /* HardFault Status           */
    volatile uint32_t bfar = SCB->BFAR; /* Bus Fault Address          */
    volatile uint32_t mmar = SCB->MMFAR;/* MemManage Fault Address    */
    (void)cfsr; (void)hfsr; (void)bfar; (void)mmar;
    while (1) {}
}