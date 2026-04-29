/**
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║                   Electrosurgical Unit                        ║
 * ╚═══════════════════════════════════════════════════════════════╝
 *
 * @file   uart_protocol.h
 * @brief  USART3 DMA + IDLE-line Nextion protocol driver.
 *
 * @details
 *  USART3 is used for Nextion display communication (9600 baud).
 *  USART1 is reserved for firmware update and must not be touched by this module.
 *
 *  Reception  : USART3_RX PB11 -> DMA1 Stream1 Ch4, circular mode.
 *               IDLE interrupt fires when the line goes silent after
 *               a burst; this marks the end of one logical packet.
 *
 *  Transmission: USART3_TX PB10 -> blocking HAL_UART_Transmit.
 *               Nextion expects each command terminated with 0xFF 0xFF 0xFF.
 */

#ifndef UART_PROTOCOL_H_
#define UART_PROTOCOL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "app_defs.h"
#include "stm32f4xx_hal.h"

#define UART_PROTO_RX_BUF_SIZE      32U
#define UART_PROTO_TX_TIMEOUT_MS    50U

/*
 * CubeMX-generated DMA handle for USART3 RX.
 * Kept with the HAL-generated name so the IRQ/DMA linkage remains valid.
 */
extern DMA_HandleTypeDef hdma_usart3_rx;

/**
  * @brief Initialise UART protocol module.
  * @param pUart USART3 handle.
  * @retval 0 on success, error code otherwise
  */
void uartProto_init(UART_HandleTypeDef* pUart);

/**
  * @brief USART3 IDLE-line callback.
  * @param None
  * @retval 0 on success, error code otherwise
  */
void uartProto_cbIdleIsr(void);

/**
  * @brief Check whether a valid settings packet has arrived.
  * @param pPkt Receives the decoded packet if available.
  * @retval true if a fresh packet is ready, false otherwise.
  */
bool uartProto_getPacket(AppDefs_EsuPacket_t* pPkt);

/**
  * @brief Send a Nextion numeric component update.
  * @param pComponent Nextion component name.
  * @param value Numeric value to send.
  * @retval 0 on success, error code otherwise
  */
void uartProto_sendVal(const char* pComponent, int32_t value);

/**
  * @brief Send a Nextion text component update.
  * @param pComponent Nextion component name.
  * @param pText Text to send.
  * @retval 0 on success, error code otherwise
  */
void uartProto_sendTxt(const char* pComponent, const char* pText);

/**
  * @brief Navigate the Nextion display to a named page.
  * @param pPageName Target page name.
  * @retval 0 on success, error code otherwise
  */
void uartProto_sendPage(const char* pPageName);

/**
  * @brief Push a complete ESU status update to the Nextion.
  * @param state ESU state.
  * @param powerDw Measured output power in deci-watts.
  * @param errors Error bitmask.
  * @retval 0 on success, error code otherwise
  */
void uartProto_pushStatus(AppDefs_EsuState_e state, uint16_t powerDw, uint8_t errors);

#ifdef __cplusplus
}
#endif

#endif /* UART_PROTOCOL_H_ */