#ifndef BASE_BT_H_
#define BASE_BT_H_

#include <zephyr/bluetooth/addr.h>
#include <zephyr/kernel.h>

/* Command IDs for base_bt cmds */
#define BASE_BT_SCAN_START 0x00U
#define BASE_BT_SCAN_STOP  0x01U
#define BASE_BT_CONN_START 0x02U
#define BASE_BT_CONN_STOP  0x03U

/* base_bt_cmd_t represents commands to the ahu_bt state machine */
typedef struct {
    uint8_t cmd_type;
    bool filter;
    bt_addr_le_t addr;
} base_bt_cmd_t;

/* Prototypes */
extern void base_bt_cmd_send(base_bt_cmd_t *cmd, k_timeout_t timeout);

#endif
