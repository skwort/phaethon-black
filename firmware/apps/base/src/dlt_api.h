#ifndef DLT_API_H_
#define DLT_API_H_

#include <zephyr/kernel.h>

/* DLT interface configuration */
#define DLT_MAX_PACKET_LEN 50 
#define DLT_PROTOCOL_BYTES 3  // don't change

/* Interface initialisation */
extern bool dlt_interface_init(uint8_t num_endpoints);
extern void dlt_device_register(k_tid_t dev_tid);
extern void dlt_link_register(uint8_t ep, k_tid_t link_tid);

/* Device functions */
extern void dlt_request(uint8_t ep, uint8_t *packet, uint8_t *data, uint8_t data_len,
                        bool async);

extern void dlt_respond(uint8_t ep, uint8_t *data, uint8_t data_len,
                        bool async);

extern uint8_t dlt_read(uint8_t ep, uint8_t *data, uint8_t data_len,
                        k_timeout_t timeout);

/* Link functions */
extern void dlt_submit(uint8_t ep, uint8_t *packet, uint8_t packet_len,
                       bool async);

extern uint8_t dlt_poll(uint8_t ep, uint8_t *packet, uint8_t packet_len,
                        k_timeout_t timeout);

#endif // DLT_API_H_