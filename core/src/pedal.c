/**
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║                   Electrosurgical Unit                        ║
 * ╚═══════════════════════════════════════════════════════════════╝
 *
 * @file   pedal.c
 * @brief  Footswitch and handswitch debounce.
 */

#include "pedal.h"

/* -------------------------------------------------------------------------- */
/* GPIO pin table                                                             */
/* -------------------------------------------------------------------------- */

typedef struct
{
    GPIO_TypeDef* pPort;
    uint16_t pin;
} Pedal_Pin_t;

static const Pedal_Pin_t gFsCutMono1 = {GPIOC, GPIO_PIN_1};
static const Pedal_Pin_t gFsCoagMono1 = {GPIOC, GPIO_PIN_8};
static const Pedal_Pin_t gFsCutMono2 = {GPIOE, GPIO_PIN_9};
static const Pedal_Pin_t gFsCoagMono2 = {GPIOE, GPIO_PIN_10};
static const Pedal_Pin_t gHsCut = {GPIOB, GPIO_PIN_4};
static const Pedal_Pin_t gHsCoag = {GPIOB, GPIO_PIN_5};
static const Pedal_Pin_t gBipoAuto = {GPIOB, GPIO_PIN_8};

/* -------------------------------------------------------------------------- */
/* Debounce counters                                                          */
/* -------------------------------------------------------------------------- */

static uint8_t gDbCutMono1 = 0U;
static uint8_t gDbCoagMono1 = 0U;
static uint8_t gDbCutMono2 = 0U;
static uint8_t gDbCoagMono2 = 0U;
static uint8_t gDbHsCut = 0U;
static uint8_t gDbHsCoag = 0U;
static uint8_t gDbBipoAuto = 0U;

static bool gIsStableCutMono1 = false;
static bool gIsStableCoagMono1 = false;
static bool gIsStableCutMono2 = false;
static bool gIsStableCoagMono2 = false;
static bool gIsStableHsCut = false;
static bool gIsStableHsCoag = false;
static bool gIsStableBipoAuto = false;

/* -------------------------------------------------------------------------- */
/* Helpers                                                                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief Read one active-LOW input.
 * @param pPin Pointer to pin descriptor.
 * @retval true when input is asserted, false otherwise.
 */
static bool pedal_read(const Pedal_Pin_t* pPin) {
    return (GPIO_PIN_RESET == HAL_GPIO_ReadPin(pPin->pPort, pPin->pin));
}

/**
 * @brief Update one debounce channel.
 * @param isRawPressed Raw active state.
 * @param pCount Debounce counter.
 * @param pIsStable Stable state output.
 * @retval None
 */
static void pedal_debounce(bool isRawPressed, uint8_t* pCount, bool* pIsStable) {
    if (true == isRawPressed) {
        if (*pCount < PEDAL_DEBOUNCE_TICKS) {
            (*pCount)++;
        }
    }
    else {
        *pCount = 0U;
    }

    *pIsStable = (*pCount >= PEDAL_DEBOUNCE_TICKS);
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief Initialise pedal module.
 * @param None
 * @retval 0 on success, error code otherwise
 */
void pedal_init(void) {
    gDbCutMono1 = 0U;
    gDbCoagMono1 = 0U;
    gDbCutMono2 = 0U;
    gDbCoagMono2 = 0U;
    gDbHsCut = 0U;
    gDbHsCoag = 0U;
    gDbBipoAuto = 0U;

    gIsStableCutMono1 = false;
    gIsStableCoagMono1 = false;
    gIsStableCutMono2 = false;
    gIsStableCoagMono2 = false;
    gIsStableHsCut = false;
    gIsStableHsCoag = false;
    gIsStableBipoAuto = false;
}

/**
 * @brief Update debounce counters.
 * @param None
 * @retval 0 on success, error code otherwise
 */
void pedal_update(void) {
    pedal_debounce(pedal_read(&gFsCutMono1), &gDbCutMono1, &gIsStableCutMono1);
    pedal_debounce(pedal_read(&gFsCoagMono1), &gDbCoagMono1, &gIsStableCoagMono1);
    pedal_debounce(pedal_read(&gFsCutMono2), &gDbCutMono2, &gIsStableCutMono2);
    pedal_debounce(pedal_read(&gFsCoagMono2), &gDbCoagMono2, &gIsStableCoagMono2);
    pedal_debounce(pedal_read(&gHsCut), &gDbHsCut, &gIsStableHsCut);
    pedal_debounce(pedal_read(&gHsCoag), &gDbHsCoag, &gIsStableHsCoag);
    pedal_debounce(pedal_read(&gBipoAuto), &gDbBipoAuto, &gIsStableBipoAuto);
}

/**
 * @brief Return true when the CUT input is stably pressed.
 * @param channel Channel to query.
 * @retval true if CUT is pressed, false otherwise.
 */
bool pedal_isCutPressed(AppDefs_Channel_e channel) {
    switch (channel) {
        case AppDefs_Channel_Mono1: {
            return (gIsStableCutMono1 || gIsStableHsCut);
        }

        case AppDefs_Channel_Mono2: {
            return (gIsStableCutMono2 || gIsStableHsCut);
        }

        case AppDefs_Channel_Bipolar: {
            return (gIsStableCutMono1 || gIsStableCutMono2);
        }

        default: {
            return false;
        }
    }
}

/**
 * @brief Return true when the COAG input is stably pressed.
 * @param channel Channel to query.
 * @retval true if COAG is pressed, false otherwise.
 */
bool pedal_isCoagPressed(AppDefs_Channel_e channel) {
    switch (channel) {
    case AppDefs_Channel_Mono1: {
        return (gIsStableCoagMono1 || gIsStableHsCoag);
    }

    case AppDefs_Channel_Mono2: {
        return (gIsStableCoagMono2 || gIsStableHsCoag);
    }

    case AppDefs_Channel_Bipolar: {
        return (gIsStableCoagMono1 || gIsStableCoagMono2);
    }

    default:
        return false;
    }
}

/**
 * @brief Return true when bipolar forceps auto-start contact is detected.
 * @param None
 * @retval true if bipolar auto-start is active, false otherwise.
 */
bool pedal_isBipolarAuto(void) {
    return gIsStableBipoAuto;
}