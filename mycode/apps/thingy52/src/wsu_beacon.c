/* Only include this code if we are using the standard implementation */
#ifndef CONFIG_BT_EXT_ADV

#include <stdio.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>

#include "wsu_msg_api.h"
#include "zephyr/bluetooth/gap.h"

LOG_MODULE_REGISTER(wsu_beacon_module, LOG_LEVEL_INF);

/* Thread parameters */
#define T_BEACON_STACKSIZE 1024
#define T_BEACON_PRIORITY  7

/* BLE parameters */
#define WSU_BEACON_RSSI 0xc8

/* Indexes into the ad and bt_data structs */
#define WSU_BT_DATA_MANU_DATA_IDX    1
#define WSU_BT_DATA_SEQ_START_IDX   10
#define WSU_BT_DATA_PITCH_START_IDX 12
#define WSU_BT_DATA_ROLL_START_IDX  16
#define WSU_BT_DATA_YAW_START_IDX   20

/* LED node */
#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

/* Define bt_data structs for each advertisement */
static struct bt_data wsu_data_ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
    BT_DATA_BYTES(BT_DATA_MANUFACTURER_DATA, 0x4c, 0x00, /* Apple */
                  0x02, 0x15,                                /* iBeacon */
                  0x18, 0xee,                              /* UUID*/
                  0x15, 0x16,                              /* UUID[15..12] */
                  0x01, 0x6b,                              /* UUID[2..0] */
                  0x4b, 0xec,                            /* Sequence */
                  0xad, 0x96,                            /* Pitch Upper */
                  0xbc, 0xb9,                            /* Pitch Lower */
                  0x6d, 0x99,                            /* Roll Upper */
                  0x00, 0x00,                            /* Roll Lower  */
                  0x00, 0x00,                            /* Yaw Upper   */
                  0x00, 0x00,                            /* Yaw Lower  */
                  WSU_BEACON_RSSI) /* Calibrated RSSI @ 1m */
};

/* Set the advertising parameters, ensuring fixed MAC identity */
static const struct bt_le_adv_param wsu_adv_param = {
    .id = BT_ID_DEFAULT,
    .sid = 0U,
    .secondary_max_skip = 0U,
    .options = BT_LE_ADV_OPT_USE_IDENTITY,
    .interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
    .interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
    .peer = NULL,
};

/*
 * The method used to load the data into the bt_data ad is sort of dirty. It
 * involves casting the struct bt_data::data member (a const variable) to a
 * non-const variable. This is fine here as the const member does not occupy
 * read-only memory, but it seems like a shortcoming in the API.
 */
bool wsu_update_bt_adv_data(float *msg)
{
    
    uint8_t *bt_ad;

    /* Type-pun the msg */
    union {
        uint32_t  u;
        float     f;
    } m;

    /* Update the BLE ad data */
    bt_ad = (uint8_t *)wsu_data_ad[WSU_BT_DATA_MANU_DATA_IDX].data;
    m.f = msg[0];
    bt_ad[WSU_BT_DATA_PITCH_START_IDX + 0] = (m.u >> 24) & 0xFF;
    bt_ad[WSU_BT_DATA_PITCH_START_IDX + 1] = (m.u >> 16) & 0xFF;
    bt_ad[WSU_BT_DATA_PITCH_START_IDX + 2] = (m.u >> 8) & 0xFF;
    bt_ad[WSU_BT_DATA_PITCH_START_IDX + 3] = m.u & 0xFF;
    m.f = msg[1];
    bt_ad[WSU_BT_DATA_ROLL_START_IDX + 0] = (m.u >> 24) & 0xFF;
    bt_ad[WSU_BT_DATA_ROLL_START_IDX + 1] = (m.u >> 16) & 0xFF;
    bt_ad[WSU_BT_DATA_ROLL_START_IDX + 2] = (m.u >> 8) & 0xFF;
    bt_ad[WSU_BT_DATA_ROLL_START_IDX + 3] = m.u & 0xFF;
    m.f = msg[2];
    bt_ad[WSU_BT_DATA_YAW_START_IDX + 0] = (m.u >> 24) & 0xFF;
    bt_ad[WSU_BT_DATA_YAW_START_IDX + 1] = (m.u >> 16) & 0xFF;
    bt_ad[WSU_BT_DATA_YAW_START_IDX + 2] = (m.u >> 8) & 0xFF;
    bt_ad[WSU_BT_DATA_YAW_START_IDX + 3] = m.u & 0xFF;

    int err = bt_le_adv_update_data(wsu_data_ad, ARRAY_SIZE(wsu_data_ad), NULL, 0);
    if (err) {
        return false;
    }

    return true;
}

bool wsu_start_bt_broadcast(void)
{
    int err;

    /* Initialize the Bluetooth Subsystem */
    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)\n", err);
        return false;
    }

    /* Start advertising */
    err = bt_le_adv_start(&wsu_adv_param,
                          wsu_data_ad,
                          ARRAY_SIZE(wsu_data_ad),
                          NULL, 0);
    if (err) {
        printk("Advertising failed to start (err %d)\n", err);
        return 0;
    }
    return true;
}

/* Initialisation function for the beacon thread */
bool wsu_beacon_init()
{

    /* Initialise the LED */
    if (!gpio_is_ready_dt(&led)) {
        return false;
    }

    int ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        return false;
    }

    /* Initialise bluetooth broadcast */
    if (!wsu_start_bt_broadcast()) {
        LOG_ERR("Failed to start BT broadcast.");
        return false;
    }

    return true;
}

/*
 * Beacon thread for transmitting sensor data over BLE
 */
void wsu_beacon_thread(void)
{
    LOG_INF("Initialising WSU Beacon.");
    if (!wsu_beacon_init()) {
        LOG_INF("Initialisation failed.");
        return;
    }

    LOG_INF("Initialisation successful.");

    float msg[3];

    while (1) {
        /* Wait for a wsu message, then process it */
        wsu_msg_recv(msg, K_FOREVER);

        printk("\np=%f r=%f y=%f", (double)msg[0], (double)msg[1], (double)msg[2]);

        /* Update BLE Advertisment */
        if (!wsu_update_bt_adv_data(msg)) {
            LOG_ERR("Error updating bt adv data. Stopping.");
            break;
        }

        k_sleep(K_MSEC(5));
    }
}

K_THREAD_DEFINE(wsu_beacon, T_BEACON_STACKSIZE, wsu_beacon_thread, NULL, NULL,
                NULL, T_BEACON_PRIORITY, 0, 0);

#endif