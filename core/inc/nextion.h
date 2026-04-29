/**
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║                   Electrosurgical Unit                        ║
 * ╚═══════════════════════════════════════════════════════════════╝
 * @file    nextion.h
 * @brief   Nextion display driver — USART3 DMA + IDLE-line detection.
 *
 * @details
 *  Reception   :   USART3_RX  PB11, DMA1 Stream1 Ch4, circular, IDLE IRQ.
 *  Transmission:   USART3_TX  PB10, blocking HAL_UART_Transmit (short strings)
 *
 *  Protocol Nextion → STM32:
 *    Binary ESU_Packet_t (12 bytes, header 0xAA, XOR checksum)
 *
 *  Protocol STM32 → Nextion:
 *    ASCII Nextion component writes terminated with 0xFF 0xFF 0xFF.
 *    Examples:
 *      "state.val=1\xff\xff\xff"   — current ESU state index
 *      "pwr.val=1234\xff\xff\xff"  — measured power
 *      "err.val=0\xff\xff\xff"     — error flags
 *      "page main\xff\xff\xff"     — page navigation
 */

#ifndef _NEXTION_H
#define _NEXTION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <stdio.h>
#include "app_defs.h"
#include "stm32f4xx_hal.h"

#define NEXTION_RX_BUF_SIZE   256U
#define NEXTION_TX_TIMEOUT_MS 50U

extern DMA_HandleTypeDef hdma_usart3_rx;

/**
 * @brief  Initialise Nextion driver
 *         Enables IDLE interrupt and starts DMA circular receive
 * @param  pHuart  Must be &huart3 (USART3, 9600 baud)
 */
void nextion_init(UART_HandleTypeDef *pHuart);

/**
 * @brief  Call from USART3_IRQHandler when IDLE flag is set
 *         Copies DMA buffer, validates packet, sets internal ready flag
 */
void nextion_idleCallback(void);

/**
 * @brief  Check whether a valid settings packet has arrived
 * @param[out] pPkt  Filled with the received settings
 * @return true if a new packet is ready (flag cleared on return)
 */
bool nextion_getPacket(ESU_Packet_t *pPkt);

/**
 * @brief  Send a Nextion component value update.
 *         ex: nextion_sendVal("state", 1) → "state.val=1\xff\xff\xff"
 */
void nextion_sendVal(const char *pComponent, int32_t value);

/**
 * @brief  Send a Nextion page-change command
 *         ex: nextion_sendPage("pageMain") → "page pageMain\xff\xff\xff"
 */
void nextion_sendPage(const char *pPageName);

/**
 * @brief  Push current ESU state to the display
 *         Sends state, measured power and error flags
 */
void nextion_pushStatus(uint8_t state, uint16_t power_dw, uint8_t errors);

#ifdef __cplusplus
}
#endif

#endif /* _NEXTION_H */