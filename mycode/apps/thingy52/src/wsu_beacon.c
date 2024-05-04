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
#define WSU_BLE_MAX_ADV 3
#define WSU_BEACON_RSSI 0xc8

/* Indexes into the ad and bt_data structs */
#define WSU_PITCH_AD_IDX            0
#define WSU_ROLL_AD_IDX             1
#define WSU_YAW_AD_IDX              2
#define WSU_BT_DATA_MANU_DATA_IDX   1
#define WSU_BT_DATA_MAJOR_START_IDX 20

/* LED node */
#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

/* Define bt_data structs for each advertisement */
static struct bt_data wsu_pitch[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
    BT_DATA_BYTES(BT_DATA_MANUFACTURER_DATA, 0x4c, 0x00, /* Apple */
                  0x02, 0x15,                            /* iBeacon */
                  0x18, 0xee, 0x15, 0x16,                /* UUID[15..12] */
                  0x01, 0x6b,                            /* UUID[11..10] */
                  0x4b, 0xec,                            /* UUID[9..8] */
                  0xad, 0x96,                            /* UUID[7..6] */
                  0xbc, 0xb9, 0x6d, 0x99, 0x99, 0x01,    /* UUID[5..0] */
                  0x00, 0x00,                            /* Major */
                  0x00, 0x00,                            /* Minor */
                  WSU_BEACON_RSSI) /* Calibrated RSSI @ 1m */
};

static struct bt_data wsu_roll[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
    BT_DATA_BYTES(BT_DATA_MANUFACTURER_DATA, 0x4c, 0x00, /* Apple */
                  0x02, 0x15,                            /* iBeacon */
                  0x18, 0xee, 0x15, 0x16,                /* UUID[15..12] */
                  0x01, 0x6b,                            /* UUID[11..10] */
                  0x4b, 0xec,                            /* UUID[9..8] */
                  0xad, 0x96,                            /* UUID[7..6] */
                  0xbc, 0xb9, 0x6d, 0x99, 0x99, 0x02,    /* UUID[5..0] */
                  0x00, 0x00,                            /* Major */
                  0x00, 0x00,                            /* Minor */
                  WSU_BEACON_RSSI) /* Calibrated RSSI @ 1m */
};

static struct bt_data wsu_yaw[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
    BT_DATA_BYTES(BT_DATA_MANUFACTURER_DATA, 0x4c, 0x00, /* Apple */
                  0x02, 0x15,                            /* iBeacon */
                  0x18, 0xee, 0x15, 0x16,                /* UUID[15..12] */
                  0x01, 0x6b,                            /* UUID[11..10] */
                  0x4b, 0xec,                            /* UUID[9..8] */
                  0xad, 0x96,                            /* UUID[7..6] */
                  0xbc, 0xb9, 0x6d, 0x99, 0x99, 0x03,    /* UUID[5..0] */
                  0x00, 0x00,                            /* Major */
                  0x00, 0x00,                            /* Minor */
                  WSU_BEACON_RSSI) /* Calibrated RSSI @ 1m */
};


/* Array of BLE Ext Ads */
static struct bt_data *ad[] = {
    wsu_pitch,
    wsu_roll,
    wsu_yaw,
};

static struct bt_le_ext_adv *adv[WSU_BLE_MAX_ADV];

/*
 * The method used to load the data into the bt_data ads is sort of dirty. It
 * involves casting the struct bt_data::data member (a const variable) to a
 * non-const variable. This is fine here as the const member does not occupy
 * read-only memory, but it seems like a short-coming in the API.
 */
void wsu_update_bt_adv_data(float *msg)
{
    uint8_t *bt_ad;

    /* Type-pun the msg */
    union {
        uint32_t  u;
        float     f;
    } m;

    /* Load the packet into the bt_data struct for broadcast. */
    bt_ad = (uint8_t *)wsu_pitch[WSU_BT_DATA_MANU_DATA_IDX].data;
    m.f = msg[0];
    bt_ad[WSU_BT_DATA_MAJOR_START_IDX + 0] = (m.u >> 24) & 0xFF;
    bt_ad[WSU_BT_DATA_MAJOR_START_IDX + 1] = (m.u >> 16) & 0xFF;
    bt_ad[WSU_BT_DATA_MAJOR_START_IDX + 2] = (m.u >> 8) & 0xFF;
    bt_ad[WSU_BT_DATA_MAJOR_START_IDX + 3] = m.u & 0xFF;

    /* Reload the updated ad data inside the BLE driver */
    bt_le_ext_adv_set_data(adv[WSU_PITCH_AD_IDX], wsu_pitch,
                            ARRAY_SIZE(wsu_pitch), NULL, 0);

    LOG_INF("Updated bt_adv_data for pitch");

    /* Repeat for roll data */
    bt_ad = (uint8_t *)wsu_roll[WSU_BT_DATA_MANU_DATA_IDX].data;
    m.f = msg[1];
    bt_ad[WSU_BT_DATA_MAJOR_START_IDX + 0] = (m.u >> 24) & 0xFF;
    bt_ad[WSU_BT_DATA_MAJOR_START_IDX + 1] = (m.u >> 16) & 0xFF;
    bt_ad[WSU_BT_DATA_MAJOR_START_IDX + 2] = (m.u >> 8) & 0xFF;
    bt_ad[WSU_BT_DATA_MAJOR_START_IDX + 3] = m.u & 0xFF;

    bt_le_ext_adv_set_data(adv[WSU_ROLL_AD_IDX], wsu_roll,
                            ARRAY_SIZE(wsu_roll), NULL, 0);

    LOG_INF("Updated bt_adv_data for roll");

    /* Repeat for yaw data */
    bt_ad = (uint8_t *)wsu_yaw[WSU_BT_DATA_MANU_DATA_IDX].data;
    m.f = msg[2];
    bt_ad[WSU_BT_DATA_MAJOR_START_IDX + 0] = (m.u >> 24) & 0xFF;
    bt_ad[WSU_BT_DATA_MAJOR_START_IDX + 1] = (m.u >> 16) & 0xFF;
    bt_ad[WSU_BT_DATA_MAJOR_START_IDX + 2] = (m.u >> 8) & 0xFF;
    bt_ad[WSU_BT_DATA_MAJOR_START_IDX + 3] = m.u & 0xFF;

    bt_le_ext_adv_set_data(adv[WSU_YAW_AD_IDX], wsu_yaw,
                            ARRAY_SIZE(wsu_yaw), NULL, 0);

    LOG_INF("Updated bt_adv_data for yaw");

    return;
}

bool wsu_start_bt_broadcast(void)
{
    /* Set the advertising parameters, ensuring fixed MAC identity */
    struct bt_le_adv_param adv_param = {
        .id = BT_ID_DEFAULT,
        .sid = 0U,
        .secondary_max_skip = 0U,
        .options = BT_LE_ADV_OPT_EXT_ADV | BT_LE_ADV_OPT_USE_IDENTITY,
        .interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
        .interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
        .peer = NULL,
    };
    int err;

    /* Initialize the Bluetooth Subsystem */
    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)\n", err);
        return false;
    }

    /* Create and start the BLE extended advertising set */
    for (int index = 0; index < WSU_BLE_MAX_ADV; index++) {
        /* Use advertising set instance index as SID */
        adv_param.sid = index;

        /* Create a non-connectable non-scannable advertising set */
        err = bt_le_ext_adv_create(&adv_param, NULL, &adv[index]);
        if (err) {
            LOG_ERR("Failed to create advertising set %d (err %d)\n", index,
                    err);
            return false;
        }

        /* Set extended advertising data - same length for each BLE packet */
        err = bt_le_ext_adv_set_data(adv[index], ad[index],
                                     ARRAY_SIZE(wsu_pitch), NULL, 0);
        if (err) {
            LOG_ERR("Failed to set advertising data for set %d "
                    "(err %d)\n",
                    index, err);
            return false;
        }

        /* Start extended advertising set */
        err = bt_le_ext_adv_start(adv[index], BT_LE_EXT_ADV_START_DEFAULT);
        if (err) {
            LOG_ERR("Failed to start extended advertising set %d "
                    "(err %d)\n",
                    index, err);
            return false;
        }

        LOG_INF("Started Extended Advertising Set %d.\n", index);
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
        wsu_update_bt_adv_data(msg);

        k_sleep(K_MSEC(5));
    }
}

K_THREAD_DEFINE(wsu_beacon, T_BEACON_STACKSIZE, wsu_beacon_thread, NULL, NULL,
                NULL, T_BEACON_PRIORITY, 0, 0);