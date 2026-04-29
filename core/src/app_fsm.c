/**
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║                   Electrosurgical Unit                        ║
 * ╚═══════════════════════════════════════════════════════════════╝
 * @file    app_fsm.c
 * @brief   ESU state machine and RF output control interface
 *
 * @details
 *  RF output control strategy
 *      The carrier PWM timers run continuously.  Output is gated by enabling
 *      or disabling the amp-enable GPIO (ESU_GPIO_CUT_EN etc.) so the RF
 *      path to the patient is only live during active cutting/coagulation.
 *
 *      For PURE CUT the carrier (TIM4 CH1, 500 kHz) runs with a fixed 50 %
 *      duty cycle at all times; the power amplifier gain is controlled by the
 *      DAC voltage (PA4) which is updated by the closed-loop P-controller
 *      running off ADC feedback.
 *
 *      For BLEND modes an additional burst modulation is applied:
 *      TIM2 (33 kHz interrupt) toggles TIM4 CCR between power_duty and 0 to
 *      create the envelope duty required for each blend crest factor:
 *          Blend1: 35 %  (CF 1.7)
 *          Blend2: 23 %  (CF 2.1)
 *          Blend3: 15 %  (CF 2.6)
 *
 *      COAG modes use TIM13 CH1 (400 kHz) as carrier:
 *          Soft:    39 % duty, continuous
 *          Contact: 3.4% duty, 23 kHz (implemented by setting TIM13 CCR low)
 *          Spray:   3.1% duty, 33 kHz (same approach)
 *          Argon:   same waveform as Spray
 *
 *  Power feedback
 *      ADC1 runs in scan mode, 5 channels:
 *          CH0 (PA0) = current sense 1
 *          CH1 (PA1) = voltage sense
 *          CH2 (PA2) = current sense 2 (neutral/return)
 *          CH3 (PA3) = REM sense (impedance divider)
 *          CH4 (PB0) = temperature sense (NTC)
 *
 *      The power controller runs every 10 ms (TIM5 at 100 Hz) to stay
 *      well below the tissue's thermal time constant.
 *
 *  Audio
 *      TIM9 CH2 (PE6) generates the buzzer tone. Two frequencies:
 *          CUT active:  ~800 Hz  (TIM9 ARR = 124, at 84 MHz / 210)
 *          COAG active: ~500 Hz  (TIM9 ARR = 199)
 *          Default/off: buzzer disabled
 *
 *  Polypectomy
 *      TIM5 ISR at 100 Hz provides a 10 ms tick.
 *      State alternates between ESU_STATE_POLYPECTOMY_CUT (30-50 ms) and
 *      ESU_STATE_POLYPECTOMY_COAG (100-1500 ms depending on poly_level).
 *      CUT phase uses TIM4 (350 kHz, set by changing prescaler).
 *      COAG phase uses TIM13 at 400 kHz, low duty.
 */

#include "app_fsm.h"

/* ----------------------------------------------------------------
 * Timer period constants  (carrier generation)
 *   System clock 168 MHz, APB1/2 prescaler = 4
 *   → APB timers clock = 84 MHz
 * ----------------------------------------------------------------*/

// TIM4: CUT carrier 500 kHz  (PSC=20, ARR=7)
#define TIM4_PSC            20U
#define TIM4_ARR             7U
#define TIM4_DUTY_50PCT      4U     // 50 % of ARR+1

/* TIM13: COAG carrier 400 kHz  (PSC=20, ARR=9) */
#define TIM13_PSC           20U
#define TIM13_ARR            9U
/* Duty cycles for coag modes */
#define TIM13_DUTY_SOFT      4U     // 39 % → CCR = 4 of 10
#define TIM13_DUTY_CONTACT   1U     // 10 % (≈CF 5.4 approximation, hardware PA does the rest)
#define TIM13_DUTY_SPRAY     1U     // 10 % (≈CF 5.7)

/* TIM9: audio buzzer (PSC=209, so tick = 84 MHz / 210 = 400 kHz) */
#define TIM9_TICK_HZ        400000UL
#define TIM9_FREQ_CUT_HZ    800UL
#define TIM9_FREQ_COAG_HZ   500UL
#define TIM9_ARR_CUT        (TIM9_TICK_HZ / TIM9_FREQ_CUT_HZ  - 1U)   // 499
#define TIM9_ARR_COAG       (TIM9_TICK_HZ / TIM9_FREQ_COAG_HZ - 1U)   // 799
#define TIM9_DUTY_HALF(arr) (((arr) + 1U) / 2U)

/* Blend burst duty counts  (over TIM2 33 kHz period → 1 tick = 1 cycle)
 * We implement a software counter that runs N cycles total and enables
 * the carrier for M of those cycles:
 *   Blend1: 35 % → 7 on / 13 off out of 20 total
 *   Blend2: 23 % → 5 on / 17 off out of 22 total
 *   Blend3: 15 % → 3 on / 17 off out of 20 total
 */
#define BLEND1_ON    7U
#define BLEND1_TOTAL 20U
#define BLEND2_ON    5U
#define BLEND2_TOTAL 22U
#define BLEND3_ON    3U
#define BLEND3_TOTAL 20U

// REM thresholds (raw 12-bit ADC, adjust to hardware divider)
#define REM_ADC_MIN  200U   // below this → disconnected (alarm)
#define REM_ADC_MAX  3800U  // above this → short (alarm)

// Over-current threshold (raw ADC)
#define OVERCURRENT_ADC_THR 3900U

// Over-temperature threshold (raw ADC — NTC thermistor)
#define OVERTEMP_ADC_THR    3500U   // tune to actual NTC curve

// Debounce: footswitch must be stable for N polls (5 ms each ≈ 25 ms)
#define FS_DEBOUNCE_TICKS   5U

/* ----------------------------------------------------------------
 * Internal state
 * ----------------------------------------------------------------*/
static ADC_HandleTypeDef  *_hadc       = NULL;
static DAC_HandleTypeDef  *_hdac       = NULL;
static TIM_HandleTypeDef  *_htim_cut   = NULL;   // TIM4
static TIM_HandleTypeDef  *_htim_coag  = NULL;   // TIM13
static TIM_HandleTypeDef  *_htim_coag2 = NULL;   // TIM14
static TIM_HandleTypeDef  *_htim_audio = NULL;   // TIM9
static TIM_HandleTypeDef  *_htim_mod   = NULL;   // TIM2
static TIM_HandleTypeDef  *_htim_poly  = NULL;   // TIM5

static ESU_State_e   _state      = ESU_STATE_IDLE;
static ESU_Settings_t _settings  = {0};
static ESU_Channel_e  _channel   = CHANNEL_MONO1;
static uint8_t        _errors    = ESU_ERR_NONE;

/* Power control */
static uint32_t  _dac_val        = 0U;    // current DAC output (0-4095)
static uint16_t  _adc_raw[ADC_CHANNELS];  // last ADC scan results
static uint16_t  _power_meas_dw  = 0U;   // measured power ×10 (deci-watts)

/* Polypectomy internal counter */
static uint16_t _poly_tick_count = 0U;
static uint16_t _poly_cut_ticks  = 4U;   // 40 ms default
static uint16_t _poly_coag_ticks = 50U;  // 500 ms default

/* Blend modulation counter (incremented in TIM2 ISR) */
static volatile uint8_t _blend_counter = 0U;
static volatile uint8_t _blend_on      = BLEND1_ON;
static volatile uint8_t _blend_total   = BLEND1_TOTAL;
static volatile bool     _blend_active  = false;

/* Footswitch debounce counters */
static uint8_t _fs_cut_db   = 0U;
static uint8_t _fs_coag_db  = 0U;
static bool    _fs_cut_prev  = false;
static bool    _fs_coag_prev = false;

/* ----------------------------------------------------------------
 * Private prototypes
 * ----------------------------------------------------------------*/
static void     _rf_cut_start(void);
static void     _rf_cut_stop(void);
static void     _rf_coag_start(void);
static void     _rf_coag_stop(void);
static void     _audio_start(bool is_cut);
static void     _audio_stop(void);
static void     _led_set(GPIO_TypeDef *port, uint16_t pin, bool on);
static void     _all_outputs_off(void);
static void     _read_adc(void);
static void     _power_control_loop(void);
static uint16_t _compute_power_dw(void);
static bool     _rem_ok(void);
static bool     _is_cut_switch_pressed(void);
static bool     _is_coag_switch_pressed(void);
static bool     _is_bipolar_auto(void);
static void     _enter_state(ESU_State_e new_state);
static void     _configure_blend(CutMode_e mode);
static void     _dac_set(uint32_t val);

/* ----------------------------------------------------------------
 * Inline GPIO helpers
 * ----------------------------------------------------------------*/
static inline void _gpio_on(GPIO_TypeDef *p, uint16_t pin)  { p->BSRR = (uint32_t)pin; }
static inline void _gpio_off(GPIO_TypeDef *p, uint16_t pin) { p->BSRR = (uint32_t)pin << 16U; }

/* ----------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------*/
/**
 * @brief   Initialise the ESU module. Call once after all HAL inits
 * @param   pHadc    Pointer to ADC1 handle
 * @param   pHdac    Pointer to DAC handle
 * @param   pHtim_cut    TIM4  (CUT carrier, 500 kHz)
 * @param   pHtim_coag   TIM13 (COAG carrier, 400 kHz)
 * @param   pHtim_coag2  TIM14 (Contact/Spray carrier, ≥400 kHz)
 * @param   pHtim_audio  TIM9  (buzzer)
 * @param   pHtim_mod    TIM2  (Blend1 envelope, 33 kHz, interrupt)
 * @param   pHtim_poly   TIM5  (polypectomy tick, 100 Hz, interrupt)
 */
void esu_init(ADC_HandleTypeDef   *pHadc,
              DAC_HandleTypeDef   *pHdac,
              TIM_HandleTypeDef   *pHtim_cut,
              TIM_HandleTypeDef   *pHtim_coag,
              TIM_HandleTypeDef   *pHtim_coag2,
              TIM_HandleTypeDef   *pHtim_audio,
              TIM_HandleTypeDef   *pHtim_mod,
              TIM_HandleTypeDef   *pHtim_poly) {
    
    _hadc       = pHadc;
    _hdac       = pHdac;
    _htim_cut   = pHtim_cut;
    _htim_coag  = pHtim_coag;
    _htim_coag2 = pHtim_coag2;
    _htim_audio = pHtim_audio;
    _htim_mod   = pHtim_mod;
    _htim_poly  = pHtim_poly;

    /* Default settings: Mono1, Pure Cut 30 W, Soft Coag 30 W, level 1 */
    memset(&_settings, 0, sizeof(_settings));
    _settings.cut_mode    = CUT_MODE_PURE;
    _settings.cut_level   = 1;
    _settings.cut_power_w = 30;
    _settings.coag_mode   = COAG_MODE_SOFT;
    _settings.coag_level  = 1;
    _settings.coag_power_w = 30;
    _channel = CHANNEL_MONO1;

    /* All outputs off, LEDs off */
    _all_outputs_off();
    _dac_set(0);

    /* Start carrier timers (output enable pins are LOW, so RF is not live yet) */
    HAL_TIM_PWM_Start(_htim_cut,   TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(_htim_coag,  TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(_htim_coag2, TIM_CHANNEL_1);

    /* Blend modulation timer: start in interrupt mode, not PWM */
    HAL_TIM_Base_Start_IT(_htim_mod);   // TIM2 @ 33 kHz

    /* Polypectomy tick: start in interrupt mode */
    HAL_TIM_Base_Start_IT(_htim_poly);  // TIM5 @ 100 Hz

    /* ADC: start continuous scan */
    HAL_ADC_Start(_hadc);

    _state = ESU_STATE_IDLE;
}

/**
 * @brief   Apply a new settings packet received from Nextion
 * @note    Does not activate output — just stores settings
 */
void esu_applySettings(const ESU_Packet_t *pkt)
{
    /* Packet already validated by nextion driver */
    _channel                  = (ESU_Channel_e)pkt->channel;
    _settings.cut_mode        = pkt->cut_mode;
    _settings.cut_level       = pkt->cut_level;
    _settings.cut_power_w     = pkt->cut_power_w;
    _settings.coag_mode       = pkt->coag_mode;
    _settings.coag_level      = pkt->coag_level;
    _settings.coag_power_w    = pkt->coag_power_w;
    _settings.poly_level      = pkt->poly_level;

    /* Pre-compute polypectomy timing */
    _poly_cut_ticks  = 4U;   // 40 ms (middle of 30-50 ms range)
    _poly_coag_ticks = POLY_COAG_TICKS(_settings.poly_level);

    /* If currently idle, also reconfigure the blend envelope parameters */
    if (_state == ESU_STATE_IDLE) {
        _configure_blend((CutMode_e)_settings.cut_mode);
    }
}

/**
 * @brief   Run one iteration of the ESU main loop
 * @details Call from while(1) in main().
 *          Polls footswitches/handswitches and handles state transitions.
 */
void esu_process(void)
{
    /* ---- 1. Read ADC and check safety conditions ---- */
    _read_adc();

    /* REM check (skip for bipolar) */
    if (_channel != CHANNEL_BIPOLAR && !_rem_ok()) {
        if (_state != ESU_STATE_REM_ALARM && _state != ESU_STATE_ERROR) {
            _enter_state(ESU_STATE_REM_ALARM);
        }
        return;
    }
    /* If we recovered from REM alarm go back to IDLE */
    if (_state == ESU_STATE_REM_ALARM && _rem_ok()) {
        _enter_state(ESU_STATE_IDLE);
    }

    /* Over-current safety cut */
    if (_adc_raw[ADC_CH_IDX_CURRENT1] > OVERCURRENT_ADC_THR ||
        _adc_raw[ADC_CH_IDX_CURRENT2] > OVERCURRENT_ADC_THR) {
        _errors |= ESU_ERR_OVERCURRENT;
        _enter_state(ESU_STATE_ERROR);
        return;
    }

    /* Over-temperature */
    if (_adc_raw[ADC_CH_IDX_TEMP] > OVERTEMP_ADC_THR) {
        _errors |= ESU_ERR_OVERTEMP;
        _enter_state(ESU_STATE_ERROR);
        return;
    }

    /* Error latch: stay in ERROR until re-init */
    if (_state == ESU_STATE_ERROR) {
        return;
    }

    /* ---- 2. Footswitch / handswitch debounce ---- */
    bool cut_pressed  = _is_cut_switch_pressed();
    bool coag_pressed = _is_coag_switch_pressed();

    /* Debounce CUT */
    if (cut_pressed) {
        if (_fs_cut_db < FS_DEBOUNCE_TICKS) _fs_cut_db++;
    } else {
        _fs_cut_db = 0;
    }
    /* Debounce COAG */
    if (coag_pressed) {
        if (_fs_coag_db < FS_DEBOUNCE_TICKS) _fs_coag_db++;
    } else {
        _fs_coag_db = 0;
    }

    bool cut_stable  = (_fs_cut_db  >= FS_DEBOUNCE_TICKS);
    bool coag_stable = (_fs_coag_db >= FS_DEBOUNCE_TICKS);

    /* ---- 3. State transitions ---- */
    switch (_state) {

    case ESU_STATE_IDLE:
        if (_channel == CHANNEL_BIPOLAR) {
            /* Auto-start bipolar: forceps contact detected */
            if (_is_bipolar_auto() &&
                _settings.coag_mode == BICOAG_MODE_AUTO_START) {
                _enter_state(ESU_STATE_BIPOLAR_COAG);
            } else if (cut_stable) {
                _enter_state(ESU_STATE_BIPOLAR_CUT);
            } else if (coag_stable) {
                _enter_state(ESU_STATE_BIPOLAR_COAG);
            }
        } else {
            /* Monopolar */
            if (cut_stable) {
                if (_settings.cut_mode == CUT_MODE_POLYPECTOMY) {
                    _poly_tick_count = 0;
                    _enter_state(ESU_STATE_POLYPECTOMY_CUT);
                } else {
                    _enter_state(ESU_STATE_CUT_ACTIVE);
                }
            } else if (coag_stable) {
                _enter_state(ESU_STATE_COAG_ACTIVE);
            }
        }
        break;

    case ESU_STATE_CUT_ACTIVE:
        _power_control_loop();
        if (!cut_stable) {
            _enter_state(ESU_STATE_IDLE);
        }
        break;

    case ESU_STATE_COAG_ACTIVE:
        _power_control_loop();
        if (!coag_stable) {
            _enter_state(ESU_STATE_IDLE);
        }
        break;

    case ESU_STATE_BIPOLAR_CUT:
        _power_control_loop();
        if (!cut_stable) {
            _enter_state(ESU_STATE_IDLE);
        }
        break;

    case ESU_STATE_BIPOLAR_COAG:
        _power_control_loop();
        /* Auto-start: release when no forceps contact */
        if (_settings.coag_mode == BICOAG_MODE_AUTO_START) {
            if (!_is_bipolar_auto()) {
                _enter_state(ESU_STATE_IDLE);
            }
        } else {
            if (!coag_stable) {
                _enter_state(ESU_STATE_IDLE);
            }
        }
        break;

    case ESU_STATE_POLYPECTOMY_CUT:
    case ESU_STATE_POLYPECTOMY_COAG:
        /* Polypectomy state advances in esu_polyTick() */
        _power_control_loop();
        if (!cut_stable) {
            _enter_state(ESU_STATE_IDLE);  // footswitch released → stop
        }
        break;

    default:
        /* REM_ALARM handled above; ERROR also handled above */
        break;
    }

    _fs_cut_prev  = cut_stable;
    _fs_coag_prev = coag_stable;
}

/**
 * @brief   TIM5 period-elapsed callback — polypectomy tick (100 Hz).
 * @details Call from HAL_TIM_PeriodElapsedCallback when htim == htim_poly.
 */
void esu_polyTick(void)
{
    /* Called from TIM5 ISR @ 100 Hz (every 10 ms) */
    if (_state != ESU_STATE_POLYPECTOMY_CUT &&
        _state != ESU_STATE_POLYPECTOMY_COAG) {
        return;
    }

    _poly_tick_count++;

    if (_state == ESU_STATE_POLYPECTOMY_CUT) {
        if (_poly_tick_count >= _poly_cut_ticks) {
            _poly_tick_count = 0;
            /* Switch to coag phase */
            _rf_cut_stop();
            _rf_coag_start();
            _audio_start(false);
            _state = ESU_STATE_POLYPECTOMY_COAG;
        }
    } else { /* POLYPECTOMY_COAG */
        if (_poly_tick_count >= _poly_coag_ticks) {
            _poly_tick_count = 0;
            /* Switch back to cut phase */
            _rf_coag_stop();
            _rf_cut_start();
            _audio_start(true);
            _state = ESU_STATE_POLYPECTOMY_CUT;
        }
    }
}

/**
 * @brief  TIM2 period-elapsed callback — Blend1 envelope 
 *         Toggles TIM4 CCR to implement burst modulation
 *         Call from HAL_TIM_PeriodElapsedCallback when htim == htim_mod
 */
void esu_blendTick(void)
{
    /* Called from TIM2 ISR @ 33 kHz — blend envelope modulation */
    if (!_blend_active) {
        return;
    }

    _blend_counter++;
    if (_blend_counter >= _blend_total) {
        _blend_counter = 0;
    }

    if (_blend_counter < _blend_on) {
        /* Carrier ON: restore power duty cycle */
        _htim_cut->Instance->CCR1 = TIM4_DUTY_50PCT;
    } else {
        /* Carrier OFF: zero duty */
        _htim_cut->Instance->CCR1 = 0U;
    }
}

/**
 * @brief Return current system state
 */
ESU_State_e esu_getState(void) { 
    return _state;
}
/**
 * @brief Return current error flags
 */
uint8_t esu_getErrors(void) { 
    return _errors; 
}
/**
 * @brief Return last measured output power in watts (×10 for one decimal)
 */
uint16_t esu_getMeasuredPowerDw(void) { 
    return _power_meas_dw; 
}

/**
 * @brief Force system into ERROR state (call on hardware fault)
 */
void esu_forceError(uint8_t error_flags) {
    
    _errors |= error_flags;
    _enter_state(ESU_STATE_ERROR);
}

/* ----------------------------------------------------------------
 * Private implementations
 * ----------------------------------------------------------------*/

static void _dac_set(uint32_t val)
{
    if (val > DAC_MAX) val = DAC_MAX;
    _dac_val = val;
    HAL_DAC_SetValue(_hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, val);
    HAL_DAC_Start(_hdac, DAC_CHANNEL_1);
}

static void _led_set(GPIO_TypeDef *port, uint16_t pin, bool on)
{
    if (on) _gpio_on(port, pin);
    else    _gpio_off(port, pin);
}

static void _all_outputs_off(void)
{
    /* Disable all RF amp enables */
    _gpio_off(ESU_GPIO_CUT_EN_PORT,   ESU_GPIO_CUT_EN_PIN);
    _gpio_off(ESU_GPIO_COAG_EN_PORT,  ESU_GPIO_COAG_EN_PIN);
    _gpio_off(ESU_GPIO_BIPO_EN_PORT,  ESU_GPIO_BIPO_EN_PIN);
    _gpio_off(ESU_GPIO_AUDIO_EN_PORT, ESU_GPIO_AUDIO_EN_PIN);

    /* All LEDs off */
    _led_set(ESU_GPIO_LED_CUT_PORT,  ESU_GPIO_LED_CUT_PIN,  false);
    _led_set(ESU_GPIO_LED_COAG_PORT, ESU_GPIO_LED_COAG_PIN, false);
    _led_set(ESU_GPIO_LED_REM_PORT,  ESU_GPIO_LED_REM_PIN,  false);
    _led_set(ESU_GPIO_LED_ERR_PORT,  ESU_GPIO_LED_ERR_PIN,  false);

    /* DAC to zero */
    _dac_set(0);

    /* Stop audio */
    HAL_TIM_PWM_Stop(_htim_audio, TIM_CHANNEL_2);

    /* Disable blend modulation */
    _blend_active  = false;
    _blend_counter = 0;

    /* Ensure cut carrier CCR is at 50 % (ready for next use) */
    if (_htim_cut) {
        _htim_cut->Instance->CCR1 = TIM4_DUTY_50PCT;
    }
}

static void _configure_blend(CutMode_e mode)
{
    switch (mode) {
    case CUT_MODE_BLEND1:
        _blend_on    = BLEND1_ON;
        _blend_total = BLEND1_TOTAL;
        break;
    case CUT_MODE_BLEND2:
        _blend_on    = BLEND2_ON;
        _blend_total = BLEND2_TOTAL;
        break;
    case CUT_MODE_BLEND3:
        _blend_on    = BLEND3_ON;
        _blend_total = BLEND3_TOTAL;
        break;
    default:
        _blend_on    = _blend_total; // 100 % → effectively off (not used)
        break;
    }
    _blend_counter = 0;
}

static void _rf_cut_start(void)
{
    _configure_blend((CutMode_e)_settings.cut_mode);
    _blend_active = (_settings.cut_mode == CUT_MODE_BLEND1 ||
                     _settings.cut_mode == CUT_MODE_BLEND2 ||
                     _settings.cut_mode == CUT_MODE_BLEND3);

    /* CCR at 50 % for pure / or blend will toggle in ISR */
    _htim_cut->Instance->CCR1 = TIM4_DUTY_50PCT;

    /* Enable power amplifier */
    _gpio_on(ESU_GPIO_CUT_EN_PORT, ESU_GPIO_CUT_EN_PIN);
    _led_set(ESU_GPIO_LED_CUT_PORT, ESU_GPIO_LED_CUT_PIN, true);
}

static void _rf_cut_stop(void)
{
    _blend_active = false;
    _htim_cut->Instance->CCR1 = TIM4_DUTY_50PCT;  // restore
    _gpio_off(ESU_GPIO_CUT_EN_PORT, ESU_GPIO_CUT_EN_PIN);
    _led_set(ESU_GPIO_LED_CUT_PORT, ESU_GPIO_LED_CUT_PIN, false);
}

static void _rf_coag_start(void)
{
    /* Select duty cycle based on coag mode */
    uint32_t ccr;
    if (_settings.coag_mode == COAG_MODE_SOFT) {
        ccr = TIM13_DUTY_SOFT;
    } else {
        /* Contact / Spray / Argon: low duty, high crest factor */
        ccr = TIM13_DUTY_CONTACT;
    }
    _htim_coag->Instance->CCR1 = ccr;
    _gpio_on(ESU_GPIO_COAG_EN_PORT, ESU_GPIO_COAG_EN_PIN);
    _led_set(ESU_GPIO_LED_COAG_PORT, ESU_GPIO_LED_COAG_PIN, true);
}

static void _rf_coag_stop(void)
{
    _gpio_off(ESU_GPIO_COAG_EN_PORT, ESU_GPIO_COAG_EN_PIN);
    _led_set(ESU_GPIO_LED_COAG_PORT, ESU_GPIO_LED_COAG_PIN, false);
}

static void _audio_start(bool is_cut)
{
    /* Reconfigure TIM9 ARR for cut or coag tone */
    uint32_t arr = is_cut ? TIM9_ARR_CUT : TIM9_ARR_COAG;
    __HAL_TIM_SET_AUTORELOAD(_htim_audio, arr);
    __HAL_TIM_SET_COMPARE(_htim_audio, TIM_CHANNEL_2, TIM9_DUTY_HALF(arr));

    _gpio_on(ESU_GPIO_AUDIO_EN_PORT, ESU_GPIO_AUDIO_EN_PIN);
    HAL_TIM_PWM_Start(_htim_audio, TIM_CHANNEL_2);
}

static void _audio_stop(void)
{
    HAL_TIM_PWM_Stop(_htim_audio, TIM_CHANNEL_2);
    _gpio_off(ESU_GPIO_AUDIO_EN_PORT, ESU_GPIO_AUDIO_EN_PIN);
}

static void _enter_state(ESU_State_e new_state)
{
    /* --- Exit current state --- */
    switch (_state) {
    case ESU_STATE_CUT_ACTIVE:
    case ESU_STATE_BIPOLAR_CUT:
        _rf_cut_stop();
        _audio_stop();
        _dac_set(0);
        break;
    case ESU_STATE_COAG_ACTIVE:
    case ESU_STATE_BIPOLAR_COAG:
        _rf_coag_stop();
        _audio_stop();
        _dac_set(0);
        break;
    case ESU_STATE_POLYPECTOMY_CUT:
        _rf_cut_stop();
        _audio_stop();
        _dac_set(0);
        break;
    case ESU_STATE_POLYPECTOMY_COAG:
        _rf_coag_stop();
        _audio_stop();
        _dac_set(0);
        break;
    default:
        break;
    }

    _state = new_state;

    /* --- Enter new state --- */
    switch (new_state) {
    case ESU_STATE_IDLE:
        _all_outputs_off();
        _errors = ESU_ERR_NONE;
        break;

    case ESU_STATE_CUT_ACTIVE:
    case ESU_STATE_BIPOLAR_CUT:
        _rf_cut_start();
        _audio_start(true);
        /* DAC starts at zero; power loop ramps it up */
        _dac_set(0);
        break;

    case ESU_STATE_COAG_ACTIVE:
    case ESU_STATE_BIPOLAR_COAG:
        _rf_coag_start();
        _audio_start(false);
        _dac_set(0);
        break;

    case ESU_STATE_POLYPECTOMY_CUT:
        _poly_tick_count = 0;
        _rf_cut_start();
        _audio_start(true);
        _dac_set(0);
        break;

    case ESU_STATE_POLYPECTOMY_COAG:
        /* Handled in esu_polyTick */
        break;

    case ESU_STATE_REM_ALARM:
        _all_outputs_off();
        _errors |= ESU_ERR_REM_ALARM;
        _led_set(ESU_GPIO_LED_REM_PORT, ESU_GPIO_LED_REM_PIN, true);
        break;

    case ESU_STATE_ERROR:
        _all_outputs_off();
        _led_set(ESU_GPIO_LED_ERR_PORT, ESU_GPIO_LED_ERR_PIN, true);
        break;

    default:
        break;
    }
}

/* ----------------------------------------------------------------
 * ADC / power measurement
 * ----------------------------------------------------------------*/

static void _read_adc(void)
{
    /* ADC1 is in scan + continuous mode; just poll each result */
    for (uint8_t ch = 0; ch < ADC_CHANNELS; ch++) {
        if (HAL_ADC_PollForConversion(_hadc, 1) == HAL_OK) {
            _adc_raw[ch] = (uint16_t)HAL_ADC_GetValue(_hadc);
        }
    }
}

/**
 * @brief  Compute measured output power in deci-Watts (×10).
 *
 *   V_peak = ADC_voltage × (ADC_VREF_MV / ADC_FULL_SCALE) × voltage_divider_ratio
 *   I_peak  = ADC_current × (ADC_VREF_MV / ADC_FULL_SCALE) / shunt_ohm
 *   P_rms   = (V_peak × I_peak) / 2   (sinusoidal approximation)
 *
 *  The scaling constants below (×100 and ×20) are placeholder values;
 *  tune them to match the actual voltage divider and current shunt used
 *  on the PCB.
 */
static uint16_t _compute_power_dw(void)
{
    /* Placeholder scaling — adjust to hardware */
    uint32_t v_mv = ((uint32_t)_adc_raw[ADC_CH_IDX_VOLTAGE1] * ADC_VREF_MV)
                    / ADC_FULL_SCALE;   // divider ratio = 1 here, adjust
    uint32_t i_ma = ((uint32_t)_adc_raw[ADC_CH_IDX_CURRENT1] * ADC_VREF_MV)
                    / ADC_FULL_SCALE;   // shunt gain = 1 here, adjust

    /* P_rms [mW] ≈ V_peak[mV] * I_peak[mA] / 2 (sine) / 1000 */
    uint32_t p_mw = (v_mv * i_ma) / 2000UL;

    return (uint16_t)(p_mw / 100UL);   // → deci-Watts (W × 10)
}

/**
 * @brief  Proportional power control: adjust DAC to reach target power.
 *         Called every pass through esu_process while output is active.
 */
static void _power_control_loop(void)
{
    _power_meas_dw = _compute_power_dw();

    /* Select target based on state */
    uint16_t target_w;
    if (_state == ESU_STATE_CUT_ACTIVE       ||
        _state == ESU_STATE_BIPOLAR_CUT      ||
        _state == ESU_STATE_POLYPECTOMY_CUT) {
        target_w = _settings.cut_power_w;
    } else {
        target_w = _settings.coag_power_w;
    }

    uint16_t target_dw = target_w * 10U;

    /* Error (deci-Watts) */
    int32_t err = (int32_t)target_dw - (int32_t)_power_meas_dw;

    /* Proportional adjustment */
    int32_t delta = (err * ESU_P_GAIN_NUM) / ESU_P_GAIN_DEN;

    int32_t new_dac = (int32_t)_dac_val + delta;
    if (new_dac < 0)        new_dac = 0;
    if (new_dac > DAC_MAX)  new_dac = (int32_t)DAC_MAX;

    _dac_set((uint32_t)new_dac);
}

/* ----------------------------------------------------------------
 * Safety checks
 * ----------------------------------------------------------------*/

static bool _rem_ok(void)
{
    uint16_t rem = _adc_raw[ADC_CH_IDX_REM];
    return (rem >= REM_ADC_MIN && rem <= REM_ADC_MAX);
}

/* ----------------------------------------------------------------
 * Switch detection  (active-LOW inputs with PULLUP)
 * ----------------------------------------------------------------*/

static bool _is_cut_switch_pressed(void)
{
    bool fs = (HAL_GPIO_ReadPin(ESU_GPIO_FS_CUT1_PORT,  ESU_GPIO_FS_CUT1_PIN)  == GPIO_PIN_RESET) ||
              (HAL_GPIO_ReadPin(ESU_GPIO_FS_CUT2_PORT,  ESU_GPIO_FS_CUT2_PIN)  == GPIO_PIN_RESET) ||
              (HAL_GPIO_ReadPin(ESU_GPIO_HS_CUT_PORT,   ESU_GPIO_HS_CUT_PIN)   == GPIO_PIN_RESET);
    return fs;
}

static bool _is_coag_switch_pressed(void)
{
    bool fs = (HAL_GPIO_ReadPin(ESU_GPIO_FS_COAG1_PORT, ESU_GPIO_FS_COAG1_PIN) == GPIO_PIN_RESET) ||
              (HAL_GPIO_ReadPin(ESU_GPIO_FS_COAG2_PORT, ESU_GPIO_FS_COAG2_PIN) == GPIO_PIN_RESET) ||
              (HAL_GPIO_ReadPin(ESU_GPIO_HS_COAG_PORT,  ESU_GPIO_HS_COAG_PIN)  == GPIO_PIN_RESET);
    return fs;
}

static bool _is_bipolar_auto(void)
{
    /* Forceps tip contact → pulls PB8 low through tissue impedance */
    return (HAL_GPIO_ReadPin(ESU_GPIO_BIPO_AUTO_PORT, ESU_GPIO_BIPO_AUTO_PIN) == GPIO_PIN_RESET);
}