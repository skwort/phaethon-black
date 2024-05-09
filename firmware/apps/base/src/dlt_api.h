/**
 * @file dlt_api.h
 *
 * @brief Device Link Transfer (DLT) API Header File
 *
 * This header file defines the interface for the Device Link Transfer (DLT) API.
 * DLT is a abstraction layer used for routing messages in a one-to-many fashion
 * from a Device to a Link, for transmission to a another device.
 *
 * In embedded systems applications, DLT enables developers to focus on
 * application code, rather than data passing primitives for required for 
 * message sharing. Users of the DLT protocol need only write peripheral
 * interfaces, then call the relevant DLT API functions to enable data reception
 * and transmission from separate threads.
 *
 * @note This header file should be included in projects that utilize the DLT API.
 *
 * @author Sam Kwort
 * @date 08/05/2024 
 */

#ifndef DLT_API_H_
#define DLT_API_H_

#include <zephyr/kernel.h>

/* DLT interface configuration */
#define DLT_MAX_PACKET_LEN 50 
#define DLT_PROTOCOL_BYTES 3  // don't change
#define DLT_MAX_DATA_LEN   DLT_MAX_PACKET_LEN - DLT_PROTOCOL_BYTES
#define DLT_MAX_ENDPOINTS 3

/* DLT Protocol Codes */
#define DLT_PREAMBLE 0x77
#define DLT_REQUEST_CODE 0x01
#define DLT_RESPONSE_CODE 0x02

/**
 * @brief Initializes the DLT interface with the specified number of endpoints.
 *
 * This function initializes the Device Link Transfer (DLT) interface with the
 * specified number of endpoints. Users should call this function once during
 * initialization, providing the number of endpoints to be supported.
 *
 * @param num_endpoints Number of endpoints to be supported.
 * @return true if initialization is successful, false otherwise.
 */
extern bool dlt_interface_init(uint8_t num_endpoints);

/**
 * @brief Registers the device thread with the DLT interface.
 *
 * This function registers the calling thread as a device with the DLT
 * interface. It should be called in the main thread of the device.
 *
 * @param dev_tid Thread ID of the device thread.
 */
extern void dlt_device_register(k_tid_t dev_tid);

/**
 * @brief Registers a link thread with the DLT interface.
 *
 * This function registers the calling thread as a link with the DLT interface.
 * It should be called in the thread associated with the link endpoint.
 *
 * @param ep Endpoint identifier for the link.
 * @param link_tid Thread ID of the link thread.
 */
extern void dlt_link_register(uint8_t ep, k_tid_t link_tid);

/**
 * @brief Sends a DLT request from a device to a link for data transfer.
 *
 * This function sends a DLT request from a device to a link for data transfer.
 *
 * @param ep Endpoint identifier for the link.
 * @param packet Pointer to a buffer used for storing and transferring the DLT encoded packet.
 * @param data Pointer to the data payload.
 * @param data_len Length of the data payload.
 * @param async If true, the transfer is asynchronous; otherwise, it is synchronous.
 */
extern void dlt_request(uint8_t ep, uint8_t *packet, uint8_t *data,
                        uint8_t data_len, bool async);

/**
 * @brief Sends a DLT response from a device to a link for data transfer..
 *
 * This function sends a DLT response from a device to a link for data transfer.
 *
 * @param ep Endpoint identifier for the link.
 * @param packet Pointer to a buffer used for storing and transferring the DLT encoded packet.
 * @param data Pointer to the data payload.
 * @param data_len Length of the data payload.
 * @param async If true, the transfer is asynchronous; otherwise, it is synchronous.
 */
extern void dlt_respond(uint8_t ep, uint8_t *packet, uint8_t *data,
                        uint8_t data_len, bool async);

/**
 * @brief Reads data from a link for a device.
 *
 * This function reads data from a link for a device.
 *
 * @param ep Endpoint identifier for the link.
 * @param msg_type Pointer to store the message type.
 * @param data Pointer to store the received data.
 * @param data_len Length of the buffer to store the received data.
 * @param timeout Timeout value for reading data.
 * @return Number of bytes read, or 0 if no data is available within the timeout.
 */
extern uint8_t dlt_read(uint8_t ep, uint8_t *msg_type, uint8_t *data,
                        uint8_t data_len, k_timeout_t timeout);

/**
 * @brief Submits a packet to be sent through a link.
 *
 * This function submits a packet through a link to the device.
 *
 * @param ep Endpoint identifier for the link.
 * @param packet Pointer to the packet to be sent.
 * @param packet_len Length of the packet to be sent.
 * @param async If true, the submission is asynchronous; otherwise, it is synchronous.
 */
extern void dlt_submit(uint8_t ep, uint8_t *packet, uint8_t packet_len,
                       bool async);

/**
 * @brief Polls for incoming packets on a link.
 *
 * This function polls for incoming packets on a link.
 *
 * @param ep Endpoint identifier for the link.
 * @param packet Pointer to store the received packet.
 * @param packet_len Length of the buffer to store the received packet.
 * @param timeout Timeout value for polling.
 * @return Number of bytes received, or 0 if no packet is available within the timeout.
 */
extern uint8_t dlt_poll(uint8_t ep, uint8_t *packet, uint8_t packet_len,
                        k_timeout_t timeout);

#endif // DLT_API_H_