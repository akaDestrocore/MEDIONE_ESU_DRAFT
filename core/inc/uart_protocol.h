/**
 * @file   uart_protocol.h
 * @brief  USART3 DMA + IDLE-line Nextion protocol driver.
 *
 * @details
 *  USART3 is used for Nextion display communication (9600 baud).
 *  USART1 is reserved for firmware update (boot protocol) and must
 *  NOT be touched by this module.
 *
 *  Reception  : USART3_RX  PB11 → DMA1 Stream1 Ch4, circular mode.
 *               IDLE interrupt fires when the line goes silent after
 *               a burst — this marks the end of one logical packet.
 *
 *  Transmission: USART3_TX  PB10 → blocking HAL_UART_Transmit.
 *               Nextion expects each command terminated with 0xFF 0xFF 0xFF.
 *
 *  Packet format (Nextion → STM32): ESU_Packet_t, 12 bytes.
 *  Responses   (STM32 → Nextion):  ASCII component writes, e.g.
 *      "state.val=1\xff\xff\xff"
 *      "pwr.val=123\xff\xff\xff"
 *      "err.val=0\xff\xff\xff"
 *      "page mainPage\xff\xff\xff"
 */

#ifndef _UART_PROTOCOL_H
#define _UART_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include "app_defs.h"
#include <stdbool.h>
#include <stdint.h>

#define UART_PROTO_RX_BUF    32U
#define UART_PROTO_TX_TIMEOUT 50U   // ms

/* DMA handle — extern so it can be referenced in the IRQ handler */
extern DMA_HandleTypeDef hdma_usart3_rx;

/**
 * @brief  Initialise UART protocol module.
 *         Enables IDLE interrupt and starts DMA circular receive.
 * @param  huart  Pointer to USART3 handle (huart3).
 */
void uart_proto_init(UART_HandleTypeDef *huart);

/**
 * @brief  Call from USART3_IRQHandler when IDLE flag fires.
 *         Snapshots the DMA buffer, validates and sets the ready flag.
 */
void uart_proto_idle_isr(void);

/**
 * @brief  Check whether a valid settings packet has arrived.
 * @param[out] pkt  Receives the decoded packet if available.
 * @return true if a fresh packet is ready (flag cleared on exit).
 */
bool uart_proto_get_packet(ESU_Packet_t *pkt);

/**
 * @brief  Send a Nextion numeric component update.
 *         e.g. uart_proto_send_val("state", 2) → "state.val=2\xff\xff\xff"
 */
void uart_proto_send_val(const char *component, int32_t value);

/**
 * @brief  Send a Nextion text component update.
 *         e.g. uart_proto_send_txt("mode", "Pure Cut")
 */
void uart_proto_send_txt(const char *component, const char *text);

/**
 * @brief  Navigate the Nextion display to a named page.
 *         e.g. uart_proto_send_page("mainPage")
 */
void uart_proto_send_page(const char *page_name);

/**
 * @brief  Push a complete ESU status update to the Nextion.
 *         Sends state, measured power (dW) and error flags.
 */
void uart_proto_push_status(ESU_State_e state, uint16_t power_dw, uint8_t errors);

#ifdef __cplusplus
}
#endif

#endif /* _UART_PROTOCOL_H */