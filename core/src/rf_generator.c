/**
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║                   Electrosurgical Unit                        ║
 * ╚═══════════════════════════════════════════════════════════════╝
 * @file   rf_generator.c
 * @brief  RF carrier generation — timer reconfiguration per mode
 */

#include "rf_generator.h"

/* ----------------------------------------------------------------
 * GPIO helpers
 * ----------------------------------------------------------------*/
#define _ON(port, pin)  ((port)->BSRR = (uint32_t)(pin))
#define _OFF(port, pin) ((port)->BSRR = (uint32_t)(pin) << 16U)

/**
 * @brief Output enable GPIOs
 */
#define CUT_EN_PORT     GPIOE
#define CUT_EN_PIN      GPIO_PIN_2
#define COAG_EN_PORT    GPIOE
#define COAG_EN_PIN     GPIO_PIN_3
#define BIPO_EN_PORT    GPIOE
#define BIPO_EN_PIN     GPIO_PIN_7
#define AUDIO_EN_PORT   GPIOB
#define AUDIO_EN_PIN    GPIO_PIN_1

/**
 * @brief LEDs
 */
#define LED_CUT_PORT    GPIOC
#define LED_CUT_PIN     GPIO_PIN_13
#define LED_COAG_PORT   GPIOC
#define LED_COAG_PIN    GPIO_PIN_2

/* ----------------------------------------------------------------
 * Timer frequency constants
 *   APB timer clock = 84 MHz
 *   Carrier freq = 84 MHz / (PSC+1) / (ARR+1)
 * ----------------------------------------------------------------*/
// CUT mode — TIM4 CH1
#define TIM4_PSC_500KHZ     20U   //  84M/21 = 4M, /8 = 500 kHz
#define TIM4_ARR_500KHZ     7U
#define TIM4_PSC_400KHZ     20U   // 84M/21 = 4M, /10 = 400 kHz
#define TIM4_ARR_400KHZ     9U
#define TIM4_PSC_380KHZ     21U   // 84M/22 ≈ 3.818M, /10 = 381 kHz ≈ 380 kHz
#define TIM4_ARR_380KHZ     9U
#define TIM4_PSC_350KHZ     23U   // 84M/24 = 3.5M, /10 = 350 kHz
#define TIM4_ARR_350KHZ     9U

// COAG soft / Bipolar Standard — TIM13 CH1
#define TIM13_PSC_400KHZ    20U
#define TIM13_ARR_400KHZ    9U
#define TIM13_CCR_SOFT      4U  // 39 % duty = CCR/10
#define TIM13_PSC_500KHZ    16U // 84M/17 ≈ 4.94M, /10 = 494 kHz ≈ 500 kHz
#define TIM13_ARR_500KHZ    9U
#define TIM13_CCR_HALF      5U  // 50 % duty

// COAG Contact/Spray/Argon / Bip.Forced — TIM14 CH1
#define TIM14_PSC_727KHZ    11U   // 84M/12 = 7M, /10 = 700 kHz ≈ 727 kHz
#define TIM14_ARR_727KHZ    9U
#define TIM14_CCR_LOW       1U   // ~10 % duty → high crest factor
#define TIM14_PSC_400KHZ    20U
#define TIM14_ARR_400KHZ    9U
#define TIM14_PSC_500KHZ    16U
#define TIM14_ARR_500KHZ    9U

// Audio buzzer — TIM9 CH2 (PSC=209 → tick = 84M/210 = 400 kHz)
#define TIM9_PSC         209U
#define TIM9_ARR_CUT     499U   // 400 kHz / 500 - 1 = 799 → 500 Hz. CUT: /500 = 800 Hz → ARR=499
#define TIM9_ARR_COAG    799U   // COAG: 400 kHz / 800 = 500 Hz → ARR=799

/* Blend envelope modulation counts
 *   Blend1: 35 % on  (CF 1.7)  → 7 of 20 ticks
 *   Blend2: 23 % on  (CF 2.1)  → 5 of 22 ticks
 *   Blend3: 15 % on  (CF 2.6)  → 3 of 20 ticks
 */
#define BLEND1_ON     7U
#define BLEND1_TOTAL 20U
#define BLEND2_ON     5U
#define BLEND2_TOTAL 22U
#define BLEND3_ON     3U
#define BLEND3_TOTAL 20U

// Bipolar Forced / Blend modulation
#define BIPOF_ON      5U
#define BIPOF_TOTAL  22U

/* ----------------------------------------------------------------
 * Module-private state
 * ----------------------------------------------------------------*/
static RFGen_Timers_t _t;   // copy of timer handles

// Global blend state — shared with ISR
RFGen_BlendState_t g_blend = {0};

/* Remembers which CUT timer CCR is the "full power" value
 * so the ISR can restore it during ON phase */
static volatile uint32_t _cut_ccr_full = 0U;

/* ----------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------*/

void rf_gen_init(const RFGen_Timers_t *timers) {
    
    _t = *timers;
    memset(&g_blend, 0, sizeof(g_blend));
    g_blend.active = false;

    // Start all carrier timers in PWM mode with zero CCR
    HAL_TIM_PWM_Start(_t.htim_cut,   TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(_t.htim_coag,  TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(_t.htim_coag2, TIM_CHANNEL_1);

    _t.htim_cut->Instance->CCR1   = 0;
    _t.htim_coag->Instance->CCR1  = 0;
    _t.htim_coag2->Instance->CCR1 = 0;

    // Start blend modulator and poly tick in interrupt mode
    HAL_TIM_Base_Start_IT(_t.htim_blend);
    HAL_TIM_Base_Start_IT(_t.htim_poly_tick);

    // Start slow polypectomy pulse (PWM output on TIM3 CH1) 
    // Duty will be set to ~50 % when polypectomy is active.
    HAL_TIM_PWM_Start(_t.htim_poly_slow, TIM_CHANNEL_1);
    _t.htim_poly_slow->Instance->CCR1 = 0;
}

/* ---- CUT mode ------------------------------------------------- */

void rf_gen_configure_cut(CutMode_e mode) {
    
    g_blend.active = false;

    switch (mode) {
    case CUT_MODE_PURE:
        _tim_set_freq(_t.htim_cut, TIM4_PSC_500KHZ, TIM4_ARR_500KHZ);
        _cut_ccr_full = (TIM4_ARR_500KHZ + 1U) / 2U;   // 50 % duty
        _t.htim_cut->Instance->CCR1 = _cut_ccr_full;
        break;

    case CUT_MODE_BLEND1:
        _tim_set_freq(_t.htim_cut, TIM4_PSC_400KHZ, TIM4_ARR_400KHZ);
        _cut_ccr_full = (TIM4_ARR_400KHZ + 1U) / 2U;
        _t.htim_cut->Instance->CCR1 = _cut_ccr_full;
        g_blend.on      = BLEND1_ON;
        g_blend.total   = BLEND1_TOTAL;
        g_blend.counter = 0;
        g_blend.active  = true;
        break;

    case CUT_MODE_BLEND2:
        _tim_set_freq(_t.htim_cut, TIM4_PSC_380KHZ, TIM4_ARR_380KHZ);
        _cut_ccr_full = (TIM4_ARR_380KHZ + 1U) / 2U;
        _t.htim_cut->Instance->CCR1 = _cut_ccr_full;
        g_blend.on      = BLEND2_ON;
        g_blend.total   = BLEND2_TOTAL;
        g_blend.counter = 0;
        g_blend.active  = true;
        break;

    case CUT_MODE_BLEND3:
        _tim_set_freq(_t.htim_cut, TIM4_PSC_380KHZ, TIM4_ARR_380KHZ);
        _cut_ccr_full = (TIM4_ARR_380KHZ + 1U) / 2U;
        _t.htim_cut->Instance->CCR1 = _cut_ccr_full;
        g_blend.on      = BLEND3_ON;
        g_blend.total   = BLEND3_TOTAL;
        g_blend.counter = 0;
        g_blend.active  = true;
        break;

    case CUT_MODE_POLYPECTOMY:
        /* 350 kHz carrier; polypectomy on/off is handled by TIM3 slow pulse */
        _tim_set_freq(_t.htim_cut, TIM4_PSC_350KHZ, TIM4_ARR_350KHZ);
        _cut_ccr_full = (TIM4_ARR_350KHZ + 1U) / 2U;
        _t.htim_cut->Instance->CCR1 = _cut_ccr_full;
        /* Enable TIM3 slow pulse: set ~50 % duty on the 1.5 Hz timer */
        _t.htim_poly_slow->Instance->CCR1 =
            (_t.htim_poly_slow->Instance->ARR + 1U) / 2U;
        break;

    default:
        break;
    }
}

/* ---- COAG mode ------------------------------------------------ */

void rf_gen_configure_coag(CoagMode_e mode) {
    
    g_blend.active = false;

    switch (mode) {
    case COAG_MODE_SOFT:
        /* 400 kHz, 39 % duty → CF ≈ 1.6, TIM13 */
        _tim_set_freq(_t.htim_coag, TIM13_PSC_400KHZ, TIM13_ARR_400KHZ);
        _t.htim_coag->Instance->CCR1 = TIM13_CCR_SOFT;
        break;

    case COAG_MODE_CONTACT:
        /* 727 kHz, ~10 % duty → CF ≈ 5.4, TIM14 */
        _tim_set_freq(_t.htim_coag2, TIM14_PSC_727KHZ, TIM14_ARR_727KHZ);
        _t.htim_coag2->Instance->CCR1 = TIM14_CCR_LOW;
        break;

    case COAG_MODE_SPRAY:
    case COAG_MODE_ARGON:
        /* 400 kHz, ~10 % duty → CF ≈ 5.7, TIM14 */
        _tim_set_freq(_t.htim_coag2, TIM14_PSC_400KHZ, TIM14_ARR_400KHZ);
        _t.htim_coag2->Instance->CCR1 = TIM14_CCR_LOW;
        break;

    default:
        break;
    }
}

/* ---- Bipolar CUT ---------------------------------------------- */

void rf_gen_configure_bipolar_cut(BipolarCutMode_e mode) {
    
    g_blend.active = false;

    /* Bipolar uses TIM13 for the carrier (500 kHz) */
    _tim_set_freq(_t.htim_coag, TIM13_PSC_500KHZ, TIM13_ARR_500KHZ);

    if (mode == BICUT_MODE_STANDARD) {
        /* CF 1.4, continuous */
        _t.htim_coag->Instance->CCR1 = TIM13_CCR_HALF;
    } else {
        /* BICUT_MODE_BLEND: CF 2.2, 33 kHz modulation */
        _t.htim_coag->Instance->CCR1 = TIM13_CCR_HALF;
        _cut_ccr_full = TIM13_CCR_HALF;
        g_blend.on      = BLEND1_ON;   // 35 % ≈ CF 2.2
        g_blend.total   = BLEND1_TOTAL;
        g_blend.counter = 0;
        g_blend.active  = true;
    }
}

/* ---- Bipolar COAG --------------------------------------------- */

void rf_gen_configure_bipolar_coag(BipolarCoagMode_e mode) {
    
    g_blend.active = false;

    /* Standard and Auto-Start: 500 kHz, 50 % duty, CF 1.4 */
    _tim_set_freq(_t.htim_coag, TIM13_PSC_500KHZ, TIM13_ARR_500KHZ);

    if (mode == BICOAG_MODE_FORCED) {
        /* Forced: 500 kHz, 33 kHz modulation, CF 2.2 */
        _t.htim_coag->Instance->CCR1 = TIM13_CCR_HALF;
        _cut_ccr_full = TIM13_CCR_HALF;
        g_blend.on      = BIPOF_ON;
        g_blend.total   = BIPOF_TOTAL;
        g_blend.counter = 0;
        g_blend.active  = true;
    } else {
        _t.htim_coag->Instance->CCR1 = TIM13_CCR_HALF;
    }
}

/* ---- Enable / disable paths ----------------------------------- */

void rf_gen_enable_cut(void) {
    
    _ON(CUT_EN_PORT, CUT_EN_PIN);
    _ON(LED_CUT_PORT, LED_CUT_PIN);
}

void rf_gen_disable_cut(void) {
    // Stop blend modulator first to avoid a race
    g_blend.active = false;
    _t.htim_cut->Instance->CCR1 = _cut_ccr_full; /* restore 50 % */
    _t.htim_poly_slow->Instance->CCR1 = 0;       /* stop slow pulse */
    _OFF(CUT_EN_PORT, CUT_EN_PIN);
    _OFF(LED_CUT_PORT, LED_CUT_PIN);
}

void rf_gen_enable_coag(void) {
    
    _ON(COAG_EN_PORT, COAG_EN_PIN);
    _ON(LED_COAG_PORT, LED_COAG_PIN);
}

void rf_gen_disable_coag(void) {
    g_blend.active = false;
    _OFF(COAG_EN_PORT, COAG_EN_PIN);
    _OFF(LED_COAG_PORT, LED_COAG_PIN);
}

void rf_gen_enable_bipolar(void) {
    _ON(BIPO_EN_PORT, BIPO_EN_PIN);
}

void rf_gen_disable_bipolar(void) {
    g_blend.active = false;
    _OFF(BIPO_EN_PORT, BIPO_EN_PIN);
}

void rf_gen_disable_all(void) {
    g_blend.active = false;

    _OFF(CUT_EN_PORT,   CUT_EN_PIN);
    _OFF(COAG_EN_PORT,  COAG_EN_PIN);
    _OFF(BIPO_EN_PORT,  BIPO_EN_PIN);
    _OFF(LED_CUT_PORT,  LED_CUT_PIN);
    _OFF(LED_COAG_PORT, LED_COAG_PIN);

    /* Zero all carrier CCRs */
    if (_t.htim_cut)   _t.htim_cut->Instance->CCR1   = 0;
    if (_t.htim_coag)  _t.htim_coag->Instance->CCR1  = 0;
    if (_t.htim_coag2) _t.htim_coag2->Instance->CCR1 = 0;
    if (_t.htim_poly_slow) _t.htim_poly_slow->Instance->CCR1 = 0;
}

/* ---- Audio ---------------------------------------------------- */

void rf_gen_audio_start(bool is_cut) {
    uint32_t arr = is_cut ? TIM9_ARR_CUT : TIM9_ARR_COAG;
    __HAL_TIM_SET_AUTORELOAD(_t.htim_audio, arr);
    __HAL_TIM_SET_COMPARE(_t.htim_audio, TIM_CHANNEL_2, (arr + 1U) / 2U);
    _ON(AUDIO_EN_PORT, AUDIO_EN_PIN);
    HAL_TIM_PWM_Start(_t.htim_audio, TIM_CHANNEL_2);
}

void rf_gen_audio_stop(void) {
    HAL_TIM_PWM_Stop(_t.htim_audio, TIM_CHANNEL_2);
    _OFF(AUDIO_EN_PORT, AUDIO_EN_PIN);
}

/* ---- Blend ISR ------------------------------------------------ */

/**
 * @brief  Call from HAL_TIM_PeriodElapsedCallback for TIM2 (33 kHz)
 *         or TIM3 events.  Implements software burst-mode modulation
 *         by toggling the TIM4 CCR1 between _cut_ccr_full and 0.
 */
void rf_gen_blend_tick_isr(void) {
    if (!g_blend.active) return;

    g_blend.counter++;
    if (g_blend.counter >= g_blend.total) {
        g_blend.counter = 0;
    }

    if (_t.htim_cut == NULL) return;

    if (g_blend.counter < g_blend.on) {
        _t.htim_cut->Instance->CCR1 = _cut_ccr_full;   // carrier ON
    } else {
        _t.htim_cut->Instance->CCR1 = 0U;              // carrier OFF
    }
}

/* ----------------------------------------------------------------
 * Helpers
 * ----------------------------------------------------------------*/
static void _tim_set_freq(TIM_HandleTypeDef *htim, uint32_t psc, uint32_t arr) {
    htim->Instance->CR1 &= ~TIM_CR1_CEN;    // stop
    htim->Instance->PSC = psc;
    htim->Instance->ARR = arr;
    htim->Instance->EGR = TIM_EGR_UG;       // update shadow registers
    htim->Instance->SR  &= ~TIM_SR_UIF;     // clear update flag
    htim->Instance->CR1 |=  TIM_CR1_CEN;    // restart
}

static void _tim_set_duty(TIM_HandleTypeDef *htim, uint32_t ch_ccr_reg, uint32_t ccr) {
    // Write CCR directly (channel 1 = CCR1, channel 2 = CCR2, etc.)
    *ch_ccr_reg = ccr;   // caller passes &htim->Instance->CCR1 etc.
    (void)ch_ccr_reg;
}