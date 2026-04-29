/**
 * @file   pedal.c
 * @brief  Footswitch and handswitch debounce.
 */

#include "pedal.h"

/* ----------------------------------------------------------------
 * GPIO pin table
 * ----------------------------------------------------------------*/
typedef struct { 
    GPIO_TypeDef *port; 
    uint16_t pin; 
} _Pin;

static const _Pin _FS_CUT1  = {GPIOC, GPIO_PIN_1};
static const _Pin _FS_COAG1 = {GPIOC, GPIO_PIN_8};
static const _Pin _FS_CUT2  = {GPIOE, GPIO_PIN_9};
static const _Pin _FS_COAG2 = {GPIOE, GPIO_PIN_10};
static const _Pin _HS_CUT   = {GPIOB, GPIO_PIN_4};
static const _Pin _HS_COAG  = {GPIOB, GPIO_PIN_5};
static const _Pin _BIPO_AUTO = {GPIOB, GPIO_PIN_8};

/* ----------------------------------------------------------------
 * Debounce counters
 * ----------------------------------------------------------------*/
static uint8_t _db_cut1   = 0;
static uint8_t _db_coag1  = 0;
static uint8_t _db_cut2   = 0;
static uint8_t _db_coag2  = 0;
static uint8_t _db_hs_cut = 0;
static uint8_t _db_hs_cog = 0;
static uint8_t _db_bipo   = 0;

static bool _stable_cut1  = false;
static bool _stable_coag1 = false;
static bool _stable_cut2  = false;
static bool _stable_coag2 = false;
static bool _stable_hcut  = false;
static bool _stable_hcog  = false;
static bool _stable_bipo  = false;

/* ----------------------------------------------------------------
 * Helper
 * ----------------------------------------------------------------*/
static bool _read(const _Pin *p) {
    
    /* active-LOW with pull-up */
    return (HAL_GPIO_ReadPin(p->port, p->pin) == GPIO_PIN_RESET);
}

static void _debounce(bool raw, uint8_t *cnt, bool *stable) {
    if (raw) {
        if (*cnt < PEDAL_DEBOUNCE_TICKS) (*cnt)++;
    } else {
        *cnt = 0;
    }
    *stable = (*cnt >= PEDAL_DEBOUNCE_TICKS);
}

/* ----------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------*/

void pedal_init(void)= {
    _db_cut1 = _db_coag1 = _db_cut2 = _db_coag2 = 0;
    _db_hs_cut = _db_hs_cog = _db_bipo = 0;
    _stable_cut1 = _stable_coag1 = _stable_cut2 = _stable_coag2 = false;
    _stable_hcut = _stable_hcog = _stable_bipo = false;
}

void pedal_update(void) {
    _debounce(_read(&_FS_CUT1),   &_db_cut1,   &_stable_cut1);
    _debounce(_read(&_FS_COAG1),  &_db_coag1,  &_stable_coag1);
    _debounce(_read(&_FS_CUT2),   &_db_cut2,   &_stable_cut2);
    _debounce(_read(&_FS_COAG2),  &_db_coag2,  &_stable_coag2);
    _debounce(_read(&_HS_CUT),    &_db_hs_cut, &_stable_hcut);
    _debounce(_read(&_HS_COAG),   &_db_hs_cog, &_stable_hcog);
    _debounce(_read(&_BIPO_AUTO), &_db_bipo,   &_stable_bipo);
}

bool pedal_cut_pressed(ESU_Channel_e ch) {
    switch (ch) {
    case CHANNEL_MONO1:   return _stable_cut1 || _stable_hcut;
    case CHANNEL_MONO2:   return _stable_cut2 || _stable_hcut;
    case CHANNEL_BIPOLAR: return _stable_cut1 || _stable_cut2;
    default:              return false;
    }
}

bool pedal_coag_pressed(ESU_Channel_e ch) {
    switch (ch) {
    case CHANNEL_MONO1:   return _stable_coag1 || _stable_hcog;
    case CHANNEL_MONO2:   return _stable_coag2 || _stable_hcog;
    case CHANNEL_BIPOLAR: return _stable_coag1 || _stable_coag2;
    default:              return false;
    }
}

bool pedal_bipolar_auto(void) {
    return _stable_bipo;
}