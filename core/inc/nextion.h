/**
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║                   Electrosurgical Unit                        ║
 * ╚═══════════════════════════════════════════════════════════════╝
 *
 * @file    nextion.h
 * @brief   Nextion display driver — USART3 DMA + IDLE-line detection.
 *
 * @details
 *  Reception   : USART3_RX PB11, DMA1 Stream1 Ch4, circular, IDLE IRQ.
 *  Transmission: USART3_TX PB10, blocking HAL_UART_Transmit.
 *
 *  Protocol Nextion -> STM32:
 *    Binary AppDefs_EsuPacket_t (12 bytes, header 0xAA, XOR checksum).
 *
 *  Protocol STM32 -> Nextion:
 *    ASCII Nextion component writes terminated with 0xFF 0xFF 0xFF.
 */

#ifndef NEXTION_H_
#define NEXTION_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "app_defs.h"
#include "stm32f4xx_hal.h"

#define NEXTION_RX_BUF_SIZE      256U
#define NEXTION_TX_TIMEOUT_MS    50U
#define NEXTION_PKT_HEADER      0xAAU
#define NEXTION_PKT_SIZE        (sizeof(AppDefs_EsuPacket_t))

extern DMA_HandleTypeDef hdma_usart3_rx;

/**
 * @brief  Initialise Nextion driver.
 * @param  pUart USART3 handle.
 * @retval None
 */
void nextion_init(UART_HandleTypeDef* pUart);

/**
 * @brief  USART3 IDLE-line callback.
 * @param  None
 * @retval None
 */
void nextion_cbIdleIsr(void);

/**
 * @brief  Check whether a valid settings packet has arrived.
 * @param  pPkt Receives the decoded settings packet.
 * @retval true if a new packet is ready, false otherwise.
 */
bool nextion_getPacket(AppDefs_EsuPacket_t* pPkt);

/**
 * @brief  Send a Nextion component value update.
 * @param  pComponent Nextion component name.
 * @param  value Value to send.
 * @retval None
 */
void nextion_sendVal(const char* pComponent, int32_t value);

/**
 * @brief  Send a Nextion page-change command.
 * @param  pPageName Page name.
 * @retval None
 */
void nextion_sendPage(const char* pPageName);

/**
 * @brief  Push current ESU state to the display.
 * @param  state Current ESU state.
 * @param  powerDw Measured power in deci-watts.
 * @param  errors Error flags bitmask.
 * @retval None
 */
void nextion_pushStatus(AppDefs_EsuState_e state, uint16_t powerDw, uint8_t errors);

#ifdef __cplusplus
}
#endif

#endif /* NEXTION_H_ */