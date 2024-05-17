#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/services/nus.h>

#include "dlt_api.h"
#include "dlt_endpoints.h"

/* Thread parameters */
#define DLT_NUS_STACKSIZE 2048
#define DLT_NUS_PRIORITY  6

/* Bluetooth Device Parameters */
#define DEVICE_NAME		CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN		(sizeof(DEVICE_NAME) - 1)

LOG_MODULE_REGISTER(dlt_nus_link, LOG_LEVEL_INF);

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_SRV_VAL),
};

static void notif_enabled(bool enabled, void *ctx)
{
	ARG_UNUSED(ctx);

	LOG_INF("%s() - %s\n", __func__, (enabled ? "Enabled" : "Disabled"));
}

static void received(struct bt_conn *conn, const void *data, uint16_t len, void *ctx)
{
	char message[CONFIG_BT_L2CAP_TX_MTU + 1] = "";

	ARG_UNUSED(conn);
	ARG_UNUSED(ctx);

	memcpy(message, data, MIN(sizeof(message) - 1, len));
	LOG_INF("%s() - Len: %d, Message: %s\n", __func__, len, message);
}

struct bt_nus_cb nus_listener = {
	.notif_enabled = notif_enabled,
	.received = received,
};

/* Initialise the NUS Link */
static bool dlt_nus_peripheral_init()
{
	int err = bt_nus_cb_register(&nus_listener, NULL);
	if (err) {
		LOG_ERR("Failed to register NUS callback: %d\n", err);
		return false;
	}

    err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Failed to enable bluetooth: %d\n", err);
		return false;
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("Failed to start advertising: %d\n", err);
		return false;
	}

    return true;
}

/**
 * Thread function for DLT Communication.
 * 
 * @brief Periodically polls the DLT interface for packets to transmit and 
 *        forwards any received packets.
 */
void dlt_nus_peripheral_thread(void) {
    /* Initialise the UART peripheral and register Link with DLT driver */
    k_tid_t link_tid = k_current_get();
    dlt_nus_peripheral_init();
    dlt_link_register(M5_NUS, link_tid);

	LOG_INF("Initialization complete\n");
    int err = 0;

    uint8_t dlt_recv_buf[DLT_MAX_PACKET_LEN] = {0};

    /* Sleep to let the main thread setup DLT */
    k_sleep(K_MSEC(100));

	while (true) {
        /* Poll DLT Interface for packets to transmit */
        uint8_t msg_len = dlt_poll(M5_NUS, dlt_recv_buf, 50, K_MSEC(5));
        if (msg_len) {
            LOG_INF("DLT mail received.");

            /* Transmit the DLT packet via UART */
            LOG_INF("Transmitting DLT packet.");
            for (int i = 0; i < msg_len; i++) {
                LOG_INF("  packet[%i]: %" PRIx8, i, dlt_recv_buf[i]);
            }
            err = bt_nus_send(NULL, dlt_recv_buf, msg_len);
            LOG_INF("Data send - Result: %d\n", err);
            if (err < 0 && (err != -EAGAIN) && (err != -ENOTCONN)) {
                LOG_ERR("Unknown error. Aborting.");
                return;
            }
        }
        k_sleep(K_MSEC(5));
	}

}

/* Register the thread */
K_THREAD_DEFINE(dlt_nus_peripheral, DLT_NUS_STACKSIZE, dlt_nus_peripheral_thread,
                NULL, NULL, NULL, DLT_NUS_PRIORITY, 0, 0);