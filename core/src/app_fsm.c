/**
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║                   Electrosurgical Unit                        ║
 * ╚═══════════════════════════════════════════════════════════════╝
 * 
 * @file   app_fsm.c
 * @brief  ESU application state machine.
 *
 * @details
 *  This module owns the ESU system state and orchestrates all other
 *  modules.  It does NOT contain any hardware register access —
 *  that is the job of rf_generator, adc_monitor, pedal and uart_protocol.
 *
 *  Call sequence from main():
 *    app_fsm_init(...)   — once, after all HAL inits
 *    loop:
 *      app_fsm_process() — main loop body
 *
 *  ISR hooks:
 *    app_fsm_poly_tick() — from HAL_TIM_PeriodElapsedCallback (TIM5)
 *    app_fsm_blend_tick()— from HAL_TIM_PeriodElapsedCallback (TIM2)
 *    app_fsm_idle_isr()  — from USART3_IRQHandler on IDLE flag
 */

#include "app_fsm.h"
#include "rf_generator.h"
#include "adc_monitor.h"
#include "safety.h"
#include "pedal.h"
#include "uart_protocol.h"
#include <string.h>

/* ----------------------------------------------------------------
 * Private state
 * ----------------------------------------------------------------*/
static ESU_State_e    _state        = ESU_STATE_IDLE;
static ESU_Channel_e  _channel      = CHANNEL_MONO1;
static ESU_Settings_t _settings     = {0};
static uint8_t        _errors       = ESU_ERR_NONE;

/* Polypectomy timing counters (driven by TIM5 at 100 Hz) */
static uint16_t _poly_tick  = 0;
static uint16_t _poly_cut_t = 4;    // 40 ms
static uint16_t _poly_cog_t = 50;   // 500 ms

/* Nextion status is pushed only when state or measured power changes */
static ESU_State_e _last_sent_state  = (ESU_State_e)0xFF;
static uint16_t    _last_sent_pwr_dw = 0xFFFF;

/* ----------------------------------------------------------------
 * Private helpers
 * ----------------------------------------------------------------*/

static void _all_off(void)
{
    rf_gen_disable_all();
    rf_gen_audio_stop();
    adc_monitor_dac_zero();
}

static void _enter_state(ESU_State_e new_state)
{
    /* ---- Exit actions ---- */
    switch (_state) {
    case ESU_STATE_CUT_ACTIVE:
    case ESU_STATE_POLYPECTOMY_CUT:
        rf_gen_disable_cut();
        rf_gen_audio_stop();
        adc_monitor_dac_zero();
        break;
    case ESU_STATE_COAG_ACTIVE:
    case ESU_STATE_POLYPECTOMY_COAG:
        rf_gen_disable_coag();
        rf_gen_audio_stop();
        adc_monitor_dac_zero();
        break;
    case ESU_STATE_BIPOLAR_CUT:
        rf_gen_disable_bipolar();
        rf_gen_audio_stop();
        adc_monitor_dac_zero();
        break;
    case ESU_STATE_BIPOLAR_COAG:
        rf_gen_disable_bipolar();
        rf_gen_audio_stop();
        adc_monitor_dac_zero();
        break;
    default:
        break;
    }

    _state = new_state;

    /* ---- Entry actions ---- */
    switch (new_state) {

    case ESU_STATE_IDLE:
        _all_off();
        /* Clear latched errors that were caused by the now-idle output */
        _errors &= ~(ESU_ERR_OVERCURRENT | ESU_ERR_OVERTEMP);
        uart_proto_send_page("mainPage");
        break;

    case ESU_STATE_CUT_ACTIVE:
        rf_gen_configure_cut((CutMode_e)_settings.cut_mode);
        rf_gen_enable_cut();
        rf_gen_audio_start(true);
        uart_proto_send_page("activePage");
        break;

    case ESU_STATE_COAG_ACTIVE:
        rf_gen_configure_coag((CoagMode_e)_settings.coag_mode);
        rf_gen_enable_coag();
        rf_gen_audio_start(false);
        uart_proto_send_page("activePage");
        break;

    case ESU_STATE_BIPOLAR_CUT:
        rf_gen_configure_bipolar_cut((BipolarCutMode_e)_settings.cut_mode);
        rf_gen_enable_bipolar();
        rf_gen_audio_start(true);
        break;

    case ESU_STATE_BIPOLAR_COAG:
        rf_gen_configure_bipolar_coag((BipolarCoagMode_e)_settings.coag_mode);
        rf_gen_enable_bipolar();
        rf_gen_audio_start(false);
        break;

    case ESU_STATE_POLYPECTOMY_CUT:
        _poly_tick = 0;
        rf_gen_configure_cut(CUT_MODE_POLYPECTOMY);
        rf_gen_enable_cut();
        rf_gen_audio_start(true);
        break;

    case ESU_STATE_POLYPECTOMY_COAG:
        /* Driven by poly_tick ISR; entry already handled there */
        break;

    case ESU_STATE_REM_ALARM:
        _all_off();
        _errors |= ESU_ERR_REM_ALARM;
        uart_proto_send_page("remAlarmPage");
        break;

    case ESU_STATE_ERROR:
        _all_off();
        uart_proto_send_page("errorPage");
        break;

    default:
        break;
    }
}

/* ----------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------*/

void app_fsm_init(const RFGen_Timers_t *timers,
                  ADC_HandleTypeDef    *hadc,
                  DAC_HandleTypeDef    *hdac,
                  UART_HandleTypeDef   *huart_nextion)
{
    /* Initialise sub-modules */
    rf_gen_init(timers);
    adc_monitor_init(hadc, hdac);
    pedal_init();
    uart_proto_init(huart_nextion);

    /* Default settings: Mono1, Pure Cut 30 W, Soft Coag 30 W */
    memset(&_settings, 0, sizeof(_settings));
    _settings.cut_mode     = CUT_MODE_PURE;
    _settings.cut_level    = 1;
    _settings.cut_power_w  = 30;
    _settings.coag_mode    = COAG_MODE_SOFT;
    _settings.coag_level   = 1;
    _settings.coag_power_w = 30;
    _channel = CHANNEL_MONO1;

    _state  = ESU_STATE_IDLE;
    _errors = ESU_ERR_NONE;

    _all_off();

    /* Greet the display */
    uart_proto_send_page("mainPage");
}

void app_fsm_process(void)
{
    /* 1. Scan ADC */
    adc_monitor_scan();

    /* 2. Update debounce */
    pedal_update();

    /* 3. Receive settings from Nextion */
    ESU_Packet_t pkt;
    if (uart_proto_get_packet(&pkt)) {
        /* Only update when idle — prevent mid-operation reconfiguration */
        if (_state == ESU_STATE_IDLE) {
            _channel              = (ESU_Channel_e)pkt.channel;
            _settings.cut_mode    = pkt.cut_mode;
            _settings.cut_level   = pkt.cut_level;
            _settings.cut_power_w = pkt.cut_power_w;
            _settings.coag_mode   = pkt.coag_mode;
            _settings.coag_level  = pkt.coag_level;
            _settings.coag_power_w= pkt.coag_power_w;
            _settings.poly_level  = pkt.poly_level;

            /* Pre-compute polypectomy timing from level */
            _poly_cut_t = 4U;   // 40 ms
            _poly_cog_t = POLY_COAG_TICKS(_settings.poly_level);
        }
    }

    /* 4. Safety checks */
    bool is_bipolar = (_channel == CHANNEL_BIPOLAR);
    Safety_Fault_e faults = safety_check(is_bipolar);

    if (faults & SAFETY_FAULT_REM) {
        if (_state != ESU_STATE_REM_ALARM && _state != ESU_STATE_ERROR) {
            _enter_state(ESU_STATE_REM_ALARM);
        }
        goto push_status;
    }
    /* REM recovered */
    if (_state == ESU_STATE_REM_ALARM && !(faults & SAFETY_FAULT_REM)) {
        _enter_state(ESU_STATE_IDLE);
    }

    if (faults & SAFETY_FAULT_OC) {
        _errors |= ESU_ERR_OVERCURRENT;
        _enter_state(ESU_STATE_ERROR);
        goto push_status;
    }
    if (faults & SAFETY_FAULT_OT) {
        _errors |= ESU_ERR_OVERTEMP;
        _enter_state(ESU_STATE_ERROR);
        goto push_status;
    }

    if (_state == ESU_STATE_ERROR) goto push_status;

    /* 5. Switch state machine */
    bool cut_on  = pedal_cut_pressed(_channel);
    bool coag_on = pedal_coag_pressed(_channel);

    switch (_state) {

    case ESU_STATE_IDLE:
        if (is_bipolar) {
            if (pedal_bipolar_auto() &&
                _settings.coag_mode == BICOAG_MODE_AUTO_START) {
                _enter_state(ESU_STATE_BIPOLAR_COAG);
            } else if (cut_on) {
                _enter_state(ESU_STATE_BIPOLAR_CUT);
            } else if (coag_on) {
                _enter_state(ESU_STATE_BIPOLAR_COAG);
            }
        } else {
            if (cut_on) {
                if (_settings.cut_mode == CUT_MODE_POLYPECTOMY) {
                    _enter_state(ESU_STATE_POLYPECTOMY_CUT);
                } else {
                    _enter_state(ESU_STATE_CUT_ACTIVE);
                }
            } else if (coag_on) {
                _enter_state(ESU_STATE_COAG_ACTIVE);
            }
        }
        break;

    case ESU_STATE_CUT_ACTIVE:
        adc_monitor_power_loop(_settings.cut_power_w);
        if (!cut_on) _enter_state(ESU_STATE_IDLE);
        break;

    case ESU_STATE_COAG_ACTIVE:
        adc_monitor_power_loop(_settings.coag_power_w);
        if (!coag_on) _enter_state(ESU_STATE_IDLE);
        break;

    case ESU_STATE_BIPOLAR_CUT:
        adc_monitor_power_loop(_settings.cut_power_w);
        if (!cut_on) _enter_state(ESU_STATE_IDLE);
        break;

    case ESU_STATE_BIPOLAR_COAG:
        adc_monitor_power_loop(_settings.coag_power_w);
        if (_settings.coag_mode == BICOAG_MODE_AUTO_START) {
            if (!pedal_bipolar_auto()) _enter_state(ESU_STATE_IDLE);
        } else {
            if (!coag_on) _enter_state(ESU_STATE_IDLE);
        }
        break;

    case ESU_STATE_POLYPECTOMY_CUT:
    case ESU_STATE_POLYPECTOMY_COAG: {
        /* Choose correct target power for the current sub-phase */
        uint16_t pw = (_state == ESU_STATE_POLYPECTOMY_CUT)
                      ? _settings.cut_power_w
                      : _settings.coag_power_w;
        adc_monitor_power_loop(pw);
        /* Foot release always stops polypectomy immediately */
        if (!cut_on) _enter_state(ESU_STATE_IDLE);
        break;
    }

    default:
        break;
    }

push_status:;
    /* 6. Push status to Nextion only when something changed */
    uint16_t pwr_dw = adc_monitor_get_power_dw();
    if (_state != _last_sent_state || pwr_dw != _last_sent_pwr_dw) {
        uart_proto_push_status(_state, pwr_dw, _errors);
        _last_sent_state  = _state;
        _last_sent_pwr_dw = pwr_dw;
    }
}

void app_fsm_poly_tick(void) {
    if (_state != ESU_STATE_POLYPECTOMY_CUT &&
        _state != ESU_STATE_POLYPECTOMY_COAG) return;

    _poly_tick++;

    if (_state == ESU_STATE_POLYPECTOMY_CUT) {
        if (_poly_tick >= _poly_cut_t) {
            _poly_tick = 0;
            rf_gen_disable_cut();
            rf_gen_configure_coag(COAG_MODE_SOFT);
            rf_gen_enable_coag();
            rf_gen_audio_start(false);
            _state = ESU_STATE_POLYPECTOMY_COAG;
        }
    } else {
        if (_poly_tick >= _poly_cog_t) {
            _poly_tick = 0;
            rf_gen_disable_coag();
            rf_gen_configure_cut(CUT_MODE_POLYPECTOMY);
            rf_gen_enable_cut();
            rf_gen_audio_start(true);
            _state = ESU_STATE_POLYPECTOMY_CUT;
        }
    }
}

void app_fsm_blend_tick(void) {
    rf_gen_blend_tick_isr();
}

void app_fsm_idle_isr(void) {
    uart_proto_idle_isr();
}

/* ----------------------------------------------------------------
 * Getters
 * ----------------------------------------------------------------*/
ESU_State_e app_fsm_get_state(void) { 
    return _state; 

}
uint8_t app_fsm_get_errors(void) { 
    return _errors; 
}