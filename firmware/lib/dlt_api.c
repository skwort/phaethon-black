#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "dlt_api.h"

LOG_MODULE_REGISTER(dlt_api, LOG_LEVEL_ERR);

/* Mailbox array for endpoints */
struct k_mbox eps[DLT_MAX_ENDPOINTS];
static k_tid_t link_tids[DLT_MAX_ENDPOINTS];

static k_tid_t device_tid;

/* Initialise the interface */
extern bool dlt_interface_init(uint8_t num_endpoints)
{
    if (num_endpoints > DLT_MAX_ENDPOINTS) {
        LOG_ERR("Too many endpoints.");
        return false;
    }
 
    /* Initialise the mailboxes */
    for (int i = 0; i < num_endpoints; i++) {
        k_mbox_init(&eps[i]);
    }

    return true;
}

extern void dlt_device_register(k_tid_t dev_tid)
{
    device_tid =  dev_tid;
}

extern void dlt_link_register(uint8_t ep, k_tid_t link_tid)
{
    link_tids[ep] = link_tid;
}

/* Create a packet and return its length */
static inline uint8_t dlt_generate_packet(uint8_t *packet, uint8_t msg_type,
                                          uint8_t *data, uint8_t data_len)
{
    if (data_len + DLT_PROTOCOL_BYTES > DLT_MAX_PACKET_LEN) {
        LOG_ERR("Data is too large.");
        return 0;
    }

    packet[0] = DLT_PREAMBLE;
    packet[1] = msg_type;
    packet[2] = data_len;

    /* Copy data segment into packet */ 
    for (int i = 0; i < data_len; i++) {
        packet[i + DLT_PROTOCOL_BYTES] = data[i];
    }

    return data_len + DLT_PROTOCOL_BYTES;
}

/* Generic method for sending packets via mailbox */
static inline void dlt_send(uint8_t ep, uint8_t *packet,
                            uint8_t packet_len, uint8_t msg_type,
                            k_tid_t send_tid, bool async)
{
    /* Create mailbox message */
    struct k_mbox_msg send_msg;
    send_msg.info = msg_type;
    send_msg.size = packet_len;
    send_msg.tx_data = packet;
    send_msg.tx_target_thread = send_tid;

    /* Send message asynchronously without validation */
    if (async) {
        LOG_INF("Sending async DLT message.");
        k_mbox_async_put(&eps[ep], &send_msg, NULL);
        return;
    }

    /* Send message synchronously and validate response */
    k_mbox_put(&eps[ep], &send_msg, K_FOREVER);

    if (send_msg.size < packet_len) {
        LOG_WRN("DLT message data dropped during transfer!");
        LOG_WRN("DLT receiver only had room for %d bytes", send_msg.info);
    }

}

extern void dlt_request(uint8_t ep, uint8_t *packet, uint8_t *data, uint8_t data_len, bool async)
{
    /* Create the DLT packet */
    uint8_t packet_len = dlt_generate_packet(packet, DLT_REQUEST_CODE,
                                             data, data_len);

    /* Submit the packet to the Link for transfer */
    dlt_send(ep, packet, packet_len, DLT_REQUEST_CODE, link_tids[ep], async);
}

extern void dlt_respond(uint8_t ep, uint8_t *packet, uint8_t *data, uint8_t data_len, bool async)
{
    /* Create the DLT packet */
    uint8_t packet_len = dlt_generate_packet(packet, DLT_RESPONSE_CODE,
                                             data, data_len);

    /* Submit the packet to the Link for transfer */
    dlt_send(ep, packet, packet_len, DLT_RESPONSE_CODE, link_tids[ep], async);
}

/* Submits the packet to the DLT interface for a Device to read */
extern void dlt_submit(uint8_t ep, uint8_t *packet, uint8_t packet_len,
                       bool async)
{
    uint8_t msg_type = packet[1];
    dlt_send(ep, packet, packet_len, msg_type, device_tid, async);
}

extern uint8_t dlt_read(uint8_t ep, uint8_t *msg_type, uint8_t *data,
                        uint8_t data_len, k_timeout_t timeout)
{
    struct k_mbox_msg recv_msg;
    recv_msg.size = 100;
    recv_msg.rx_source_thread = link_tids[ep];

    /* Wait to get message, but don't consume it */
    if (k_mbox_get(&eps[ep], &recv_msg, NULL, timeout)) {
        return 0;
    }

    /* Consume message and get its data */
    uint8_t rx_packet[DLT_MAX_PACKET_LEN] = {0};
    k_mbox_data_get(&recv_msg, &rx_packet);

    if (recv_msg.size > DLT_MAX_PACKET_LEN || 
            (recv_msg.size - DLT_PROTOCOL_BYTES) > data_len) {
        LOG_ERR("Message receive error. Data segment is too big.");
        return 0;
    }

    /* Get the message type */
    *msg_type = recv_msg.info;

    /* Copy the packet's data segment into data */
    for (int i = 0; i < recv_msg.size - DLT_PROTOCOL_BYTES; i++) {
        data[i] = rx_packet[i + DLT_PROTOCOL_BYTES];
    }
    return recv_msg.size - DLT_PROTOCOL_BYTES;
}

/* Polling function for Links to check if packets are available */
extern uint8_t dlt_poll(uint8_t ep, uint8_t *packet, uint8_t packet_len,
                        k_timeout_t timeout)
{
    struct k_mbox_msg recv_msg;
    recv_msg.size = 100;
    recv_msg.rx_source_thread = device_tid;

    /* Wait to get message, but don't consume it */
    if (k_mbox_get(&eps[ep], &recv_msg, NULL, timeout)) {
        return 0;
    }

    /* Consume message and get its data */
    k_mbox_data_get(&recv_msg, packet);

    if (recv_msg.size > DLT_MAX_PACKET_LEN || recv_msg.size > packet_len) {
        LOG_ERR("Message receive error. Packet is too big.");
        return 0;
    }

    return recv_msg.size;
}
