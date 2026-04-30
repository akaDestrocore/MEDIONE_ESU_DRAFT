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

#ifndef APP_DEFS_H_
#define APP_DEFS_H_

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
typedef enum
{
  AppDefs_Channel_Mono1   = 0,
  AppDefs_Channel_Mono2   = 1,
  AppDefs_Channel_Bipolar = 2,
  AppDefs_Channel_Count
} AppDefs_Channel_e;

/* ----------------------------------------------------------------
 * Monopolar CUT modes
 * ----------------------------------------------------------------*/
typedef enum
{
  AppDefs_CutMode_Pure        = 0,
  AppDefs_CutMode_Blend1      = 1,
  AppDefs_CutMode_Blend2      = 2,
  AppDefs_CutMode_Blend3      = 3,
  AppDefs_CutMode_Polypectomy = 4,
  AppDefs_CutMode_Count
} AppDefs_CutMode_e;

/* ----------------------------------------------------------------
 * Monopolar COAG modes
 * ----------------------------------------------------------------*/
typedef enum
{
  AppDefs_CoagMode_Soft    = 0,
  AppDefs_CoagMode_Contact = 1,
  AppDefs_CoagMode_Spray   = 2,
  AppDefs_CoagMode_Argon   = 3,
  AppDefs_CoagMode_Count
} AppDefs_CoagMode_e;

/* ----------------------------------------------------------------
 * Bipolar CUT modes
 * ----------------------------------------------------------------*/
typedef enum
{
  AppDefs_BipolarCutMode_Standard = 0,
  AppDefs_BipolarCutMode_Blend    = 1,
  AppDefs_BipolarCutMode_Count
} AppDefs_BipolarCutMode_e;

/* ----------------------------------------------------------------
 * Bipolar COAG modes
 * ----------------------------------------------------------------*/
typedef enum
{
  AppDefs_BipolarCoagMode_Standard  = 0,
  AppDefs_BipolarCoagMode_AutoStart = 1,
  AppDefs_BipolarCoagMode_Forced    = 2,
  AppDefs_BipolarCoagMode_Count
} AppDefs_BipolarCoagMode_e;

/* ----------------------------------------------------------------
 * System state machine
 * ----------------------------------------------------------------*/
typedef enum
{
  AppDefs_EsuState_Idle            = 0,
  AppDefs_EsuState_CutActive       = 1,
  AppDefs_EsuState_CoagActive      = 2,
  AppDefs_EsuState_BipolarCut      = 3,
  AppDefs_EsuState_BipolarCoag     = 4,
  AppDefs_EsuState_PolypectomyCut  = 5,
  AppDefs_EsuState_PolypectomyCoag = 6,
  AppDefs_EsuState_RemAlarm        = 7,
  AppDefs_EsuState_Error           = 8,
  AppDefs_EsuState_Count
} AppDefs_EsuState_e;

/* ----------------------------------------------------------------
 * REM state
 * ----------------------------------------------------------------*/
typedef enum
{
  AppDefs_RemState_Ok       = 0,
  AppDefs_RemState_Alarm    = 1,
  AppDefs_RemState_Bypassed = 2
} AppDefs_RemState_e;

/* ----------------------------------------------------------------
 * Settings for one channel
 * ----------------------------------------------------------------*/
typedef struct
{
  uint8_t  cut_mode;
  uint8_t  cut_level;
  uint16_t cut_powerW;
  uint8_t  coag_mode;
  uint8_t  coag_level;
  uint16_t coag_powerW;
  uint8_t  poly_level;
} AppDefs_EsuSettings_t;

/* ----------------------------------------------------------------
 * Binary packet from Nextion to STM32
 *
 *  Nextion Prism button event code (Touch Release):
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

#pragma pack(1)
typedef struct
{
  uint8_t  header;        /* 0xAA                */
  uint8_t  channel;       /* AppDefs_Channel_e   */
  uint8_t  cut_mode;      /* AppDefs_CutMode_e   */
  uint8_t  cut_level;     /* 1-4                 */
  uint16_t cut_powerW;    /* 0-400 W             */
  uint8_t  coag_mode;     /* AppDefs_CoagMode_e  */
  uint8_t  coag_level;    /* 1-3                 */
  uint16_t coag_powerW;   /* 0-120 W             */
  uint8_t  poly_level;    /* 1-4                 */
  uint8_t  checksum;      /* XOR                 */
} AppDefs_EsuPacket_t;
#pragma pack()

/* ----------------------------------------------------------------
 * Status reply STM32 → Nextion (ASCII, Nextion-style)
 *   "state.val=X\xff\xff\xff"
 *   "pwr.val=YYY\xff\xff\xff"   (measured watts × 10)
 *   "err.val=Z\xff\xff\xff"
 * ----------------------------------------------------------------*/

/* Error flags */
#define ESU_ERR_NONE         0x00U
#define ESU_ERR_REM_ALARM    0x01U
#define ESU_ERR_OVERTEMP     0x02U
#define ESU_ERR_OVERCURRENT  0x04U
#define ESU_ERR_PKT_BAD      0x08U

#ifdef __cplusplus
}
#endif

#endif /* APP_DEFS_H_ */