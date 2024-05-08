#ifndef BASE_BT_H_
#define BASE_BT_H_

#include <zephyr/bluetooth/addr.h>
#include <zephyr/kernel.h>

/* Command IDs for base_bt cmds */
#define BASE_BT_SCAN_START 0x00U
#define BASE_BT_SCAN_STOP  0x01U
#define BASE_BT_CONN_START 0x02U
#define BASE_BT_CONN_STOP  0x03U

#ifdef CONFIG_BT_EXT_ADV
typedef struct wsu_data_packet {
    uint8_t type;
    float value;
} wsu_data_packet;
#else
typedef struct wsu_data_packet {
    uint16_t sequence;
    float pitch;
    float roll;
    float yaw;
} wsu_data_packet;
#endif

/* base_bt_cmd_t represents commands to the ahu_bt state machine */
typedef struct {
    uint8_t cmd_type;
    bool filter;
    bt_addr_le_t addr;
} base_bt_cmd_t;

/* Prototypes */
extern void base_bt_cmd_send(base_bt_cmd_t *cmd, k_timeout_t timeout);
extern int base_bt_wsu_data_recv(wsu_data_packet *pkt, k_timeout_t timeout);

#endif
