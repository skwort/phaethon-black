#include "zephyr/bluetooth/addr.h"
#include "zephyr/bluetooth/gap.h"
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "base_bt.h"

LOG_MODULE_REGISTER(base_bt_module, LOG_LEVEL_ERR);

/* Thread parameters */
#define T_BASE_BT_STACKSIZE 1024
#define T_BASE_BT_PRIORITY  7

/* BASE BT State Machine States */
#define BASE_BT_IDLE_STATE      0
#define BASE_BT_SCANNING_STATE  1
#define BASE_BT_CONNECTED_STATE 2

/* Indexes into bt_data::data */
#define WSU_BT_DATA_MAJOR_START_IDX 20 // EXT implementation
#define WSU_BT_DATA_SEQ_START_IDX   10 
#define WSU_BT_DATA_PITCH_START_IDX 12 
#define WSU_BT_DATA_ROLL_START_IDX  16
#define WSU_BT_DATA_YAW_START_IDX   20

/* Define the message queue BT state machine commands */
K_MSGQ_DEFINE(base_bt_cmdq, sizeof(base_bt_cmd_t), 2, 1);

/* Define the message queue for passing WSU data */
K_MSGQ_DEFINE(wsu_dataq, sizeof(wsu_data_packet), 10, 1);

/* BASE BT State variable */
static uint8_t base_bt_state = BASE_BT_IDLE_STATE;

/* Rate limiting variables for HCI data forwarding */
uint64_t last_device_tx[4] = {0};

/* Send a cmd via the cmd queue */
extern void base_bt_cmd_send(base_bt_cmd_t *cmd, k_timeout_t timeout)
{
    k_msgq_put(&base_bt_cmdq, cmd, timeout);
}

/* Receive a message from the cmd queue */
static inline int base_bt_cmd_recv(base_bt_cmd_t *cmd, k_timeout_t timeout)
{
    return k_msgq_get(&base_bt_cmdq, cmd, timeout);
}

/* Send a cmd via the cmd queue */
static inline void base_bt_wsu_data_send(wsu_data_packet *pkt, k_timeout_t timeout)
{
    k_msgq_put(&wsu_dataq, pkt, timeout);
}

/* Receive a message from the cmd queue */
extern int base_bt_wsu_data_recv(wsu_data_packet *pkt, k_timeout_t timeout)
{
    return k_msgq_get(&wsu_dataq, pkt, timeout);
}

/* Initialise Bluetooth */
static inline bool base_bt_init(void)
{
    int err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)\n", err);
        return false;
    }
    return true;
}


static bool conn_data_cb(struct bt_data *data, void *user_data)
{
    /* Ignore non-manufacturer data segments */
    if (data->type != BT_DATA_MANUFACTURER_DATA) {
        /* Move to the next segment */
        return true;
    }

    /* Type-pun the packet value */
    union {
        uint32_t  u;
        float     f;
    } packet;

    #ifdef CONFIG_BT_EXT_ADV
    packet.u = 0;
    packet.u |= data->data[WSU_BT_DATA_MAJOR_START_IDX + 3];
    packet.u |= (((uint32_t)data->data[WSU_BT_DATA_MAJOR_START_IDX + 2]) << 8);
    packet.u |= (((uint32_t)data->data[WSU_BT_DATA_MAJOR_START_IDX + 1]) << 16);
    packet.u |= (((uint32_t)data->data[WSU_BT_DATA_MAJOR_START_IDX]) << 24);

    wsu_data_packet *p = user_data;
    p->type = data->data[WSU_BT_DATA_MAJOR_START_IDX - 1];
    p->value = packet.f;

    /* Print the packet */
    if (p->type == 0x01) {
        LOG_INF("pitch: %f\n", (double)p->value);
    } else if (p->type == 0x02) {
        LOG_INF("roll: %f\n", (double)p->value);
    } else if (p->type == 0x03) {
        LOG_INF("yaw: %f\n", (double)p->value);
    }
    #else
    wsu_data_packet *p = user_data;

    /* Get sequence */
    p->sequence = (((uint16_t)data->data[WSU_BT_DATA_SEQ_START_IDX]) << 8);
    p->sequence |= data->data[WSU_BT_DATA_SEQ_START_IDX + 1];

    /* Get pitch */
    packet.u = 0;
    packet.u |= data->data[WSU_BT_DATA_PITCH_START_IDX + 3];
    packet.u |= (((uint32_t)data->data[WSU_BT_DATA_PITCH_START_IDX + 2]) << 8);
    packet.u |= (((uint32_t)data->data[WSU_BT_DATA_PITCH_START_IDX + 1]) << 16);
    packet.u |= (((uint32_t)data->data[WSU_BT_DATA_PITCH_START_IDX]) << 24);
    p->pitch = packet.f;

    /* Get roll */
    packet.u = 0;
    packet.u |= data->data[WSU_BT_DATA_ROLL_START_IDX + 3];
    packet.u |= (((uint32_t)data->data[WSU_BT_DATA_ROLL_START_IDX + 2]) << 8);
    packet.u |= (((uint32_t)data->data[WSU_BT_DATA_ROLL_START_IDX + 1]) << 16);
    packet.u |= (((uint32_t)data->data[WSU_BT_DATA_ROLL_START_IDX]) << 24);
    p->roll = packet.f;

    /* Get yaw */
    packet.u = 0;
    packet.u |= data->data[WSU_BT_DATA_YAW_START_IDX + 3];
    packet.u |= (((uint32_t)data->data[WSU_BT_DATA_YAW_START_IDX + 2]) << 8);
    packet.u |= (((uint32_t)data->data[WSU_BT_DATA_YAW_START_IDX + 1]) << 16);
    packet.u |= (((uint32_t)data->data[WSU_BT_DATA_YAW_START_IDX]) << 24);
    p->yaw = packet.f;

    /* Print the packet */
    LOG_INF("seq %" PRIu16 " p %f, r %f, y %f\n", p->sequence, (double)p->pitch,
            (double)p->roll, (double)p->yaw);
    #endif

    /* Stop data processing */
    return false;
}

/* Scan callback function for handling generic scanning */
static void ble_scan_recv(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                          struct net_buf_simple *ad)
{
    if (base_bt_state != BASE_BT_SCANNING_STATE) {
        return;
    }
    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
    printk("Device found: %s (RSSI %d), type %u, AD data len %u\n", addr_str,
           rssi, type, ad->len);
}

/* Scan callback function for handling WSU beacon connections */
static void ble_conn_recv(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                          struct net_buf_simple *ad)
{
    if (base_bt_state != BASE_BT_CONNECTED_STATE) {
        return;
    }

    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));

    /* Parse the WSU data */
    wsu_data_packet packet;
    bt_data_parse(ad, conn_data_cb, &packet);
    
    /* Send it to the main thread */
    base_bt_wsu_data_send(&packet, K_MSEC(1));
    
}

/* State machine which bakes the Bluetooth API cmds into transition logic. */
void base_bt_thread(void)
{
    /* Initialise the module */
    // if (!base_bt_init()) {
    //     LOG_ERR("BASE BT init failed.");
    //     return;
    // }

    /* Set the scan parameters */
    struct bt_le_scan_param scan_param = {
        .type = BT_LE_SCAN_TYPE_PASSIVE,
        .options = BT_LE_SCAN_OPT_NONE,
        .interval = BT_GAP_SCAN_FAST_INTERVAL_MIN,
        .window = BT_GAP_SCAN_FAST_WINDOW
    };

    while (1) {
        base_bt_cmd_t cmd;
        int err;
        switch (base_bt_state) {
        case BASE_BT_IDLE_STATE:
            /* Do nothing while waiting for a command */
            base_bt_cmd_recv(&cmd, K_FOREVER);

            /* Process the command */
            if (cmd.cmd_type == BASE_BT_SCAN_START && !cmd.filter) {
                /* Start the scan */
                scan_param.options = BT_LE_SCAN_OPT_FILTER_DUPLICATE;
                err = bt_le_scan_start(&scan_param, ble_scan_recv);
                if (err) {
                    LOG_ERR("Start SCAN failed (err %d)\n", err);
                    break;
                }

                LOG_INF("Transitioning to scanning state.");
                base_bt_state = BASE_BT_SCANNING_STATE;

            } else if (cmd.cmd_type == BASE_BT_SCAN_START && cmd.filter) {
                /* Setup the filter */
                bt_le_filter_accept_list_add(&cmd.addr);
                scan_param.options |= BT_LE_SCAN_OPT_FILTER_ACCEPT_LIST;

                /* Start the scan */
                err = bt_le_scan_start(&scan_param, ble_scan_recv);
                if (err) {
                    LOG_ERR("Start SCAN failed (err %d)\n", err);
                    break;
                }

                LOG_INF("Transitioning to scanning state (filtered).");
                base_bt_state = BASE_BT_SCANNING_STATE;

            } else if (cmd.cmd_type == BASE_BT_CONN_START) {
                /* Setup the filter */
                bt_le_filter_accept_list_add(&cmd.addr);
                scan_param.options |= BT_LE_SCAN_OPT_FILTER_ACCEPT_LIST;

                /* Start the scan */
                err = bt_le_scan_start(&scan_param, ble_conn_recv);
                if (err) {
                    LOG_ERR("Start CONN failed (err %d)\n", err);
                    break;
                }

                LOG_INF("Transitioning to connected state.");
                base_bt_state = BASE_BT_CONNECTED_STATE;

            } else {
                LOG_ERR("Invalid transition cmd from IDLE state.");
            }
            break;

        case BASE_BT_SCANNING_STATE:
            /* Do nothing while waiting for a command */
            base_bt_cmd_recv(&cmd, K_FOREVER);

            /* Only accept stop commands in the SCAN state*/
            if (cmd.cmd_type == BASE_BT_SCAN_STOP && !cmd.filter) {
                /* Stop scanning */
                err = bt_le_scan_stop();
                if (err) {
                    LOG_ERR("Stop SCAN failed (err %d)\n", err);
                    break;
                }

                /* Reset the filter list */
                bt_le_filter_accept_list_clear();

                LOG_INF("Transitioning to IDLE state.");
                base_bt_state = BASE_BT_IDLE_STATE;

            } else if (cmd.cmd_type == BASE_BT_SCAN_STOP && cmd.filter) {
                /* Stop the scan */
                err = bt_le_scan_stop();
                if (err) {
                    LOG_ERR("Stop SCAN failed (err %d)\n", err);
                    break;
                }

                /* Remove the address from the filter list */
                bt_le_filter_accept_list_remove(&cmd.addr);
                scan_param.options |= BT_LE_SCAN_OPT_FILTER_ACCEPT_LIST;

                /* Restart the scan */
                err = bt_le_scan_start(&scan_param, ble_scan_recv);
                if (err) {
                    LOG_ERR("Start SCAN failed (err %d)\n", err);
                    break;
                }

            } else if (cmd.cmd_type == BASE_BT_SCAN_START && cmd.filter) {
                /* Stop the scan */
                err = bt_le_scan_stop();
                if (err) {
                    LOG_ERR("Stop SCAN failed (err %d)\n", err);
                    break;
                }

                /* Add the new filter */
                bt_le_filter_accept_list_add(&cmd.addr);
                scan_param.options |= BT_LE_SCAN_OPT_FILTER_ACCEPT_LIST;

                /* Restart the scan */
                err = bt_le_scan_start(&scan_param, ble_scan_recv);
                if (err) {
                    LOG_ERR("Start SCAN failed (err %d)\n", err);
                    break;
                }

            } else if (cmd.cmd_type == BASE_BT_SCAN_START && !cmd.filter) {
                /* Stop the scan */
                err = bt_le_scan_stop();
                if (err) {
                    LOG_ERR("Stop SCAN failed (err %d)\n", err);
                    break;
                }

                /* Reset filter list and disable filtering */
                bt_le_filter_accept_list_clear();
                scan_param.options = BT_LE_SCAN_OPT_FILTER_DUPLICATE;
                ;

                /* Restart the scan */
                err = bt_le_scan_start(&scan_param, ble_scan_recv);
                if (err) {
                    LOG_ERR("Start SCAN failed (err %d)\n", err);
                    break;
                }

            } else {
                LOG_ERR("Invalid transition cmd from SCAN state.");
            }

            break;

        case BASE_BT_CONNECTED_STATE:
            /* Wait for a command */
            base_bt_cmd_recv(&cmd, K_FOREVER);

            if (cmd.cmd_type == BASE_BT_CONN_STOP) {

                /* Stop scanning and transition back to IDLE */
                err = bt_le_scan_stop();
                if (err) {
                    LOG_ERR("Stop SCAN failed (err %d)\n", err);
                    break;
                }

                /* Reset the filter list */
                bt_le_filter_accept_list_clear();

                LOG_INF("Transitioning to IDLE state.");
                base_bt_state = BASE_BT_IDLE_STATE;

            } else {
                LOG_ERR("Invalid transition cmd from CONN state.");
            }

            break;

        default:
            /* Shouldn't get here */
            base_bt_state = BASE_BT_IDLE_STATE;
            break;
        }
    }
}

K_THREAD_DEFINE(base_bt, T_BASE_BT_STACKSIZE, base_bt_thread, NULL, NULL, NULL,
                T_BASE_BT_PRIORITY, 0, 0);