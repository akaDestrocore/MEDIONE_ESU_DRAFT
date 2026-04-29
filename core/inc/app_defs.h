/**
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║                   Electrosurgical Unit                        ║
 * ╚═══════════════════════════════════════════════════════════════╝
 * @file    app_defs.h
 * @brief   All enums, structs and constants for the ESU project.
 *
 * @details
 *  Carrier frequencies:
 *    CUT Pure/Blend1:              TIM4    CH1     PA→PD12     400–500 kHz
 *    COAG Soft / Bipolar:          TIM13   CH1     PA6         400–500 kHz
 *    COAG Contact/Spray / Bip.F:   TIM14   CH1     PA7         400–727 kHz
 *    Audio buzzer:                 TIM9    CH2     PE6         400–800 Hz
 *    Polypectomy CUT envelope:     TIM2    CH1     PA5         33 kHz
 *    Polypectomy cycle tick:       TIM5                        100 Hz
 *    Polypectomy slow pulse:       TIM3    CH1     PC6         ~1.5 Hz
 *
 *  Power control:
 *    Target power comes from Nextion packet.
 *    DAC PA4 drives the RF amplifier gain control voltage.
 *    ADC1 (PA0-PA3, PB0) continuously measures load voltage,
 *    current and REM impedance.
 *
 *  Activation:
 *    Footswitch CUT  MONO1  → PC1  (active-LOW)
 *    Footswitch COAG MONO1  → PC8  (active-LOW)
 *    Footswitch CUT  MONO2  → PE9  (active-LOW)
 *    Footswitch COAG MONO2  → PE10 (active-LOW)
 *    Handswitch CUT         → PB4  (active-LOW)
 *    Handswitch COAG        → PB5  (active-LOW)
 *    REM contact OK         → PB7  (active-HIGH)
 *    Bipolar auto-start     → PB8  (active-LOW, forceps contact)
 *
 *  Output enables:
 *    CUT  amp enable        → PE2  (active-HIGH)
 *    COAG amp enable        → PE3  (active-HIGH)
 *    Bipolar amp enable     → PE7  (active-HIGH)
 *    Audio amp enable       → PB1  (active-HIGH)
 *    LED CUT active         → PC13 (active-HIGH)
 *    LED COAG active        → PC2  (active-HIGH)
 *    LED REM alarm          → PE8  (active-HIGH)
 *    LED Error              → PC4  (active-HIGH)
 */

#ifndef _APP_DEFS_H
#define _APP_DEFS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ----------------------------------------------------------------
 * Limits
 * ----------------------------------------------------------------*/
#define ESU_MAX_CUT_POWER_W             400U
#define ESU_MAX_COAG_POWER_W            120U
#define ESU_MAX_BIPOLAR_CUT_POWER_W     120U
#define ESU_MAX_BIPOLAR_COAG_POWER_W    100U
#define ESU_MAX_LEVEL                   4U

/* ----------------------------------------------------------------
 * Channel (which monopolar output or bipolar)
 * ----------------------------------------------------------------*/
typedef enum {
    CHANNEL_MONO1   = 0,
    CHANNEL_MONO2   = 1,
    CHANNEL_BIPOLAR = 2,
    CHANNEL_COUNT
} ESU_Channel_e;

/* ----------------------------------------------------------------
 * Monopolar CUT modes
 * ----------------------------------------------------------------*/
typedef enum {
    CUT_MODE_PURE           = 0,
    CUT_MODE_BLEND1         = 1,
    CUT_MODE_BLEND2         = 2,
    CUT_MODE_BLEND3         = 3,
    CUT_MODE_POLYPECTOMY    = 4,
    CUT_MODE_COUNT
} CutMode_e;

/* ----------------------------------------------------------------
 * Monopolar COAG modes
 * ----------------------------------------------------------------*/
typedef enum {
    COAG_MODE_SOFT      = 0,
    COAG_MODE_CONTACT   = 1,
    COAG_MODE_SPRAY     = 2,
    COAG_MODE_ARGON     = 3,
    COAG_MODE_COUNT
} CoagMode_e;

/* ----------------------------------------------------------------
 * Bipolar CUT modes
 * ----------------------------------------------------------------*/
typedef enum {
    BICUT_MODE_STANDARD = 0,
    BICUT_MODE_BLEND    = 1, 
    BICUT_MODE_COUNT
} BipolarCutMode_e;

/* ----------------------------------------------------------------
 * Bipolar COAG modes
 * ----------------------------------------------------------------*/
typedef enum {
    BICOAG_MODE_STANDARD   = 0,   // Foot switch
    BICOAG_MODE_AUTO_START = 1,   // Auto start
    BICOAG_MODE_FORCED     = 2,
    BICOAG_MODE_COUNT
} BipolarCoagMode_e;

/* ----------------------------------------------------------------
 * System state machine
 * ----------------------------------------------------------------*/
typedef enum {
    ESU_STATE_IDLE              = 0,
    ESU_STATE_CUT_ACTIVE        = 1,
    ESU_STATE_COAG_ACTIVE       = 2,
    ESU_STATE_BIPOLAR_CUT       = 3,
    ESU_STATE_BIPOLAR_COAG      = 4,
    ESU_STATE_POLYPECTOMY_CUT   = 5,
    ESU_STATE_POLYPECTOMY_COAG  = 6,
    ESU_STATE_REM_ALARM         = 7,
    ESU_STATE_ERROR             = 8,
    ESU_STATE_COUNT
} ESU_State_e;

/* ----------------------------------------------------------------
 * REM state
 * ----------------------------------------------------------------*/
typedef enum {
    REM_OK        = 0,
    REM_ALARM     = 1,
    REM_BYPASSED  = 2
} REM_State_e;

/* ----------------------------------------------------------------
 * Settings for one channel
 * ----------------------------------------------------------------*/
typedef struct {
    uint8_t  cut_mode;
    uint8_t  cut_level;
    uint16_t cut_power_w;
    uint8_t  coag_mode;
    uint8_t  coag_level;
    uint16_t coag_power_w;
    uint8_t  poly_level;
} ESU_Settings_t;

/* ----------------------------------------------------------------
 * Binary packet from Nextion to STM32
 *
 *  Nextion Prism button event code (Touch Release ?):
 *    printh AA
 *    prints channel.val,    1
 *    prints cut_mode.val,   1
 *    prints cut_level.val,  1
 *    prints cut_power.val,  0   (2 bytes, LE)
 *    prints coag_mode.val,  1
 *    prints coag_level.val, 1
 *    prints coag_power.val, 0   (2 bytes, LE)
 *    prints poly_level.val, 1
 *    prints checksum.val,   1
 *
 *  Total packet = 12 bytes
 * ----------------------------------------------------------------*/
#define NEXTION_PKT_HEADER   0xAAU
#define NEXTION_PKT_SIZE     12U

#pragma pack(1)
typedef struct {
    uint8_t  header;         // 0xAA
    uint8_t  channel;        // ESU_Channel_e
    uint8_t  cut_mode;       // CutMode_e
    uint8_t  cut_level;      // 1-4
    uint16_t cut_power_w;    // 0-400 W 
    uint8_t  coag_mode;      // CoagMode_e
    uint8_t  coag_level;     // 1-3
    uint16_t coag_power_w;   // 0-120 W 
    uint8_t  poly_level;     // 1-4
    uint8_t  checksum;       // XOR
} ESU_Packet_t;
#pragma pack()

/* ----------------------------------------------------------------
 * Status reply from STM32 → Nextion (ASCII, Nextion-style)
 * STM32 sends a Nextion attribute write after each state change:
 *   "state.val=X\xff\xff\xff"
 *   "pwr.val=YYY\xff\xff\xff"    (measured watts × 10)
 *   "err.val=Z\xff\xff\xff"
 * ----------------------------------------------------------------*/

// Error flags
#define ESU_ERR_NONE         0x00U
#define ESU_ERR_REM_ALARM    0x01U
#define ESU_ERR_OVERTEMP     0x02U
#define ESU_ERR_OVERCURRENT  0x04U
#define ESU_ERR_PKT_BAD      0x08U

#ifdef __cplusplus
}
#endif

#endif /* _APP_DEFS_H */