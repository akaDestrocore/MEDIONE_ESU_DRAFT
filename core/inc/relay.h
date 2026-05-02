/**
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║                   Electrosurgical Unit                        ║
 * ╚═══════════════════════════════════════════════════════════════╝
 *
 * @file    relay.h
 * @brief   Relay and CD4051 analogue-mux routing abstraction.
 *
 * @details
 *  Maps each ESU routing configuration to a fixed GPIO snapshot
 *  stored in a compile-time lookup table.  A single call to
 *  relay_apply() atomically drives all relay and mux lines, then
 *  starts a non-blocking settle timer. The caller must poll
 *  relay_isSettled() before enabling any RF output.
 *
 *  relay_update() must be called every main-loop iteration so the
 *  settle timer advances (it reads HAL_GetTick()).
 *
 *  GPIO map
 *  ────────
 *  GPIOA  PA8  = CD4051 select A
 *         PA11 = CD4051 select B
 *         PA12 = CD4051 select C
 *  GPIOC  PC4  = Spray3 enable (relay)
 *         PC11 = Spray relay
 *  GPIOD  PD3  = M2 relay
 *         PD8  = Power-on relay
 *         PD9  = FAN enable
 *         PD11 = Monopolar relay
 *
 *  Settle time: RELAY_SETTLE_MS (200 ms)
 */

#ifndef RELAY_H_
#define RELAY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "stm32f4xx_hal.h"

// Settle time after relay switching before RF may be enabled
#define RELAY_SETTLE_MS     200U

/**
 * @brief Unified routing configuration covering all ESU operating modes.
 *
 * @note  Entries marked TODO have not been hardware-confirmed.
 *        They currently apply the RELAY_CFG_SAFE default (all off).
 *        Update the lookup table in relay.c once schematics are verified.
 */
typedef enum {
    RELAY_CFG_SAFE          = 0U,   // All relays off — startup / error state
    RELAY_CFG_CUT_MONO      = 1U,   // Monopolar CUT (Pure / Blend / Polypectomy)  TODO: verify mux
    RELAY_CFG_COAG_SOFT     = 2U,   // Monopolar COAG Soft                         TODO: verify mux
    RELAY_CFG_COAG_CONTACT  = 3U,   // Monopolar COAG Contact                      TODO: verify mux
    RELAY_CFG_COAG_SPRAY    = 4U,   // Monopolar COAG Spray  ← fully confirmed
    RELAY_CFG_COAG_ARGON    = 5U,   // Monopolar COAG Argon                        TODO: verify mux
    RELAY_CFG_BIPOLAR_CUT   = 6U,   // Bipolar CUT                                 TODO: verify mux
    RELAY_CFG_BIPOLAR_COAG  = 7U,   // Bipolar COAG                                TODO: verify mux
    RELAY_CFG_COUNT
} Relay_Config_e;

/**
 * @brief GPIO snapshot for one routing configuration.
 *
 *  Each field holds a bitmask of pins on the named port.
 *  Pins in _set are driven HIGH; pins in _reset are driven LOW.
 *  Pins absent from both masks are left unchanged.
 */
typedef struct {
    uint16_t portA_set;
    uint16_t portA_reset;
    uint16_t portC_set;
    uint16_t portC_reset;
    uint16_t portD_set;
    uint16_t portD_reset;
} Relay_GpioState_t;

/**
 * @brief  Apply a routing configuration and start the settle timer.
 * @param  config  Desired routing configuration.
 * @retval None
 */
void relay_apply(Relay_Config_e config);

/**
 * @brief  Drive all relay/mux outputs to the safe (all-off) state immediately.
 * @retval None
 */
void relay_allOff(void);

/**
 * @brief  Advance the settle timer.  Call once per main-loop iteration.
 * @retval None
 */
void relay_update(void);

/**
 * @brief  Return true when the post-relay settle time has elapsed.
 * @retval true if settled, false if still within settle window.
 */
bool relay_isSettled(void);

/**
 * @brief  Return the currently applied configuration.
 * @retval Active Relay_Config_e value.
 */
Relay_Config_e relay_getConfig(void);

#ifdef __cplusplus
}
#endif

#endif /* RELAY_H_ */