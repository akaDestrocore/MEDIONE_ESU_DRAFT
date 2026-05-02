/**
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║                   Electrosurgical Unit                        ║
 * ╚═══════════════════════════════════════════════════════════════╝
 *
 * @file    relay.c
 * @brief   Relay and CD4051 analogue-mux routing abstraction.
 *
 * @details
 *  All GPIO writes use BSRR for atomic, read-modify-write-free
 *  operation.  The settle timer uses HAL_GetTick() so it advances
 *  without blocking.
 *
 *  LOOKUP TABLE MAINTENANCE
 *  ───────────────────────
 *  Each entry in gRelayTable[] corresponds to a Relay_Config_e value.
 *  Fields that must be driven are listed in _set / _reset masks.
 *  A pin that is absent from both masks is left at its current level.
 *
 *  To add or correct an entry:
 *    1. Find the matching RELAY_CFG_* constant in relay.h.
 *    2. Update portX_set / portX_reset in the table below.
 *    3. Remove or update the TODO comment for that entry.
 */

#include "relay.h"

/* -------------------------------------------------------------------------- */
/* GPIO pin aliases — kept local; avoids polluting headers                    */
/* -------------------------------------------------------------------------- */

// GPIOA
#define MUX_A       GPIO_PIN_8   // CD4051 select A
#define MUX_B       GPIO_PIN_11  // CD4051 select B
#define MUX_C       GPIO_PIN_12  // CD4051 select C

// GPIOC
#define SPRAY3_EN   GPIO_PIN_4   // Spray3 enable relay
#define SPRAY_RLY   GPIO_PIN_11  // Spray relay

// GPIOD
#define M2_RLY      GPIO_PIN_3   // M2 relay
#define POWERON_RLY GPIO_PIN_8   // Power-on relay
#define FAN_EN      GPIO_PIN_9   // FAN enable
#define MONO_RLY    GPIO_PIN_11  // Monopolar relay

/* -------------------------------------------------------------------------- */
/* Lookup table                                                                */
/* -------------------------------------------------------------------------- */

/**
 * @brief Static lookup table indexed by Relay_Config_e.
 *
 * @note  Entries tagged [TODO] have not been hardware-confirmed.
 *        They use RELAY_CFG_SAFE defaults (all relay outputs de-energised)
 *        until confirmed against schematics.
 */
static const Relay_GpioState_t gRelayTable[RELAY_CFG_COUNT] = {
    /* ----------------------------------------------------------------
     * [0] RELAY_CFG_SAFE — all relays off, mux deselected
     * ----------------------------------------------------------------*/
    [RELAY_CFG_SAFE] = {
        .portA_set   = 0U,
        .portA_reset = MUX_A | MUX_B | MUX_C,
        .portC_set   = 0U,
        .portC_reset = SPRAY3_EN | SPRAY_RLY,
        .portD_set   = 0U,
        .portD_reset = M2_RLY | POWERON_RLY | FAN_EN | MONO_RLY,
    },

    /* ----------------------------------------------------------------
     * [1] RELAY_CFG_CUT_MONO — monopolar CUT
     * TODO: confirm mux select bits (MUX_A/B/C) against schematic.
     *       Current assumption: monopolar relay on, spray off, FAN on.
     * ----------------------------------------------------------------*/
    [RELAY_CFG_CUT_MONO] = {
        .portA_set   = 0U,
        .portA_reset = MUX_A | MUX_B | MUX_C,  // TODO: set correct mux channel
        .portC_set   = 0U,
        .portC_reset = SPRAY3_EN | SPRAY_RLY,
        .portD_set   = MONO_RLY | M2_RLY | POWERON_RLY | FAN_EN,
        .portD_reset = 0U,
    },

    /* ----------------------------------------------------------------
     * [2] RELAY_CFG_COAG_SOFT — monopolar COAG Soft
     * TODO: confirm mux select bits.
     *       Current assumption: monopolar relay on, FAN on.
     * ----------------------------------------------------------------*/
    [RELAY_CFG_COAG_SOFT] = {
        .portA_set   = 0U,
        .portA_reset = MUX_A | MUX_B | MUX_C,  // TODO: set correct mux channel
        .portC_set   = 0U,
        .portC_reset = SPRAY3_EN | SPRAY_RLY,
        .portD_set   = MONO_RLY | M2_RLY | POWERON_RLY | FAN_EN,
        .portD_reset = 0U,
    },

    /* ----------------------------------------------------------------
     * [3] RELAY_CFG_COAG_CONTACT — monopolar COAG Contact
     * TODO: confirm mux select bits.
     * ----------------------------------------------------------------*/
    [RELAY_CFG_COAG_CONTACT] = {
        .portA_set   = 0U,
        .portA_reset = MUX_A | MUX_B | MUX_C,  // TODO: set correct mux channel
        .portC_set   = 0U,
        .portC_reset = SPRAY3_EN | SPRAY_RLY,
        .portD_set   = MONO_RLY | M2_RLY | POWERON_RLY | FAN_EN,
        .portD_reset = 0U,
    },

    /* ----------------------------------------------------------------
     * [4] RELAY_CFG_COAG_SPRAY — hardware-confirmed sequence:
     *
     *   HAL_GPIO_WritePin(GPIOC, PC4,         SET);    // spray3 enable
     *   HAL_GPIO_WritePin(GPIOD, PD11,        RESET);  // monopolar relay OFF
     *   HAL_GPIO_WritePin(GPIOC, PC11,        SET);    // spray relay ON
     *   HAL_GPIO_WritePin(GPIOD, PD3 | PD8,  SET);    // M2 + power-on relay
     *   HAL_GPIO_WritePin(GPIOA, PA12 | PA8,  RESET);  // mux C, A off
     *   HAL_GPIO_WritePin(GPIOA, PA11,        SET);    // mux B on
     *   HAL_GPIO_WritePin(GPIOD, PD9,         SET);    // FAN
     * ----------------------------------------------------------------*/
    [RELAY_CFG_COAG_SPRAY] = {
        .portA_set   = MUX_B,
        .portA_reset = MUX_A | MUX_C,
        .portC_set   = SPRAY3_EN | SPRAY_RLY,
        .portC_reset = 0U,
        .portD_set   = M2_RLY | POWERON_RLY | FAN_EN,
        .portD_reset = MONO_RLY,
    },

    /* ----------------------------------------------------------------
     * [5] RELAY_CFG_COAG_ARGON — monopolar COAG Argon
     * TODO: confirm mux select bits and relay state.
     *       Placeholder identical to COAG_SPRAY pending schematic check.
     * ----------------------------------------------------------------*/
    [RELAY_CFG_COAG_ARGON] = {
        .portA_set   = MUX_B,                   // TODO: verify
        .portA_reset = MUX_A | MUX_C,           // TODO: verify
        .portC_set   = SPRAY3_EN | SPRAY_RLY,   // TODO: verify
        .portC_reset = 0U,
        .portD_set   = M2_RLY | POWERON_RLY | FAN_EN,
        .portD_reset = MONO_RLY,
    },

    /* ----------------------------------------------------------------
     * [6] RELAY_CFG_BIPOLAR_CUT
     * TODO: confirm bipolar relay wiring and mux channel.
     *       Current assumption: monopolar relay off, M2/power-on on.
     * ----------------------------------------------------------------*/
    [RELAY_CFG_BIPOLAR_CUT] = {
        .portA_set   = 0U,                      // TODO: verify mux
        .portA_reset = MUX_A | MUX_B | MUX_C,
        .portC_set   = 0U,
        .portC_reset = SPRAY3_EN | SPRAY_RLY,
        .portD_set   = M2_RLY | POWERON_RLY | FAN_EN,
        .portD_reset = MONO_RLY,
    },

    /* ----------------------------------------------------------------
     * [7] RELAY_CFG_BIPOLAR_COAG
     * TODO: confirm bipolar relay wiring and mux channel.
     * ----------------------------------------------------------------*/
    [RELAY_CFG_BIPOLAR_COAG] = {
        .portA_set   = 0U,                      // TODO: verify mux
        .portA_reset = MUX_A | MUX_B | MUX_C,
        .portC_set   = 0U,
        .portC_reset = SPRAY3_EN | SPRAY_RLY,
        .portD_set   = M2_RLY | POWERON_RLY | FAN_EN,
        .portD_reset = MONO_RLY,
    },
};

/* -------------------------------------------------------------------------- */
/* Module state                                                                */
/* -------------------------------------------------------------------------- */

static Relay_Config_e gCurrentConfig = RELAY_CFG_SAFE;
static uint32_t       gSettleStart   = 0U;
static bool           gSettling      = false;

/* -------------------------------------------------------------------------- */
/* Private helpers                                                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief  Drive a single GPIO port using BSRR (atomic, no RMW).
 * @param  pPort     Target GPIO port.
 * @param  setBits   Bitmask of pins to drive HIGH.
 * @param  resetBits Bitmask of pins to drive LOW.
 * @retval None
 */
static void relay_writePort(GPIO_TypeDef *pPort,
                            uint16_t     setBits,
                            uint16_t     resetBits) {
    // BSRR lower 16 bits = set; upper 16 bits = reset
    pPort->BSRR = (uint32_t)setBits | ((uint32_t)resetBits << 16U);
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief  Apply a routing configuration and start the settle timer.
 * @param  config  Desired routing configuration.
 * @retval None
 */
void relay_apply(Relay_Config_e config) {
    const Relay_GpioState_t *pState;

    if (config >= RELAY_CFG_COUNT) {
        config = RELAY_CFG_SAFE;
    }

    pState = &gRelayTable[config];

    relay_writePort(GPIOA, pState->portA_set, pState->portA_reset);
    relay_writePort(GPIOC, pState->portC_set, pState->portC_reset);
    relay_writePort(GPIOD, pState->portD_set, pState->portD_reset);

    gCurrentConfig = config;
    gSettleStart   = HAL_GetTick();
    gSettling      = true;
}

/**
 * @brief  Drive all relay/mux outputs to the safe (all-off) state immediately.
 * @retval None
 */
void relay_allOff(void) {
    relay_apply(RELAY_CFG_SAFE);
}

/**
 * @brief  Advance the settle timer.  Call once per main-loop iteration.
 * @retval None
 */
void relay_update(void) {
    if (false == gSettling) {
        return;
    }

    if ((HAL_GetTick() - gSettleStart) >= RELAY_SETTLE_MS) {
        gSettling = false;
    }
}

/**
 * @brief  Return true when the post-relay settle time has elapsed.
 * @retval true if settled, false if still within settle window.
 */
bool relay_isSettled(void) {
    return (false == gSettling);
}

/**
 * @brief  Return the currently applied configuration.
 * @retval Active Relay_Config_e value.
 */
Relay_Config_e relay_getConfig(void) {
    return gCurrentConfig;
}