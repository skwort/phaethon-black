

#include "zephyr/bluetooth/addr.h"
#include "zephyr/bluetooth/services/nus.h"
#include <sys/_stdint.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <zephyr/types.h>
#include <stddef.h>
#include <errno.h>
#include <zephyr/kernel.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/sys/byteorder.h>

#include "dlt_api.h"
#include "dlt_endpoints.h"

/* Thread parameters */
#define DLT_NUS_CENTRAL_STACKSIZE 2048
#define DLT_NUS_CENTRAL_PRIORITY  6

/* Define the UUIDs for the GATT Attributes */
#define BT_UUID_NUS BT_UUID_DECLARE_128(0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E)
#define BT_UUID_NUS_TX BT_UUID_DECLARE_128(0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x6E)
#define BT_UUID_NUS_RX BT_UUID_DECLARE_128(0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x03, 0x00, 0x40, 0x6E)

LOG_MODULE_REGISTER(dlt_nus_central_link, LOG_LEVEL_INF);

typedef struct nus_packet_t {
	uint8_t length;
	uint8_t data[DLT_MAX_DATA_LEN];
} nus_packet_t;

/* Define the message queue for passing data from BT driver to thread */
K_MSGQ_DEFINE(nus_msgq, sizeof(nus_packet_t), 3, 1);

// const char TARGET_ADDR_STR[] = "C8:91:07:19:03:58";  // thingy52
const char TARGET_ADDR_STR[] = "D7:BA:ED:13:75:90";  // nrfdk sam

static void start_scan(void);

static struct bt_conn *default_conn;

static struct bt_uuid_128 nus_uuid = BT_UUID_INIT_128(BT_UUID_NUS_SRV_VAL);
static struct bt_uuid_16 discover_uuid;

static struct bt_gatt_discover_params discover_params;
static struct bt_gatt_subscribe_params subscribe_params;


static uint8_t notify_func(struct bt_conn *conn,
			   struct bt_gatt_subscribe_params *params,
			   const void *data, uint16_t length)
{
	if (!data) {
		printk("[UNSUBSCRIBED]\n");
		params->value_handle = 0U;
		return BT_GATT_ITER_STOP;
	}

	printk("[NOTIFICATION] data %p length %u. Submitting.\n", data, length);

	/* Send the packet to the handler thread */
	nus_packet_t pkt;
	pkt.length = (uint8_t) length;
	memcpy(pkt.data, data, length);
	k_msgq_put(&nus_msgq, &pkt, K_NO_WAIT);


	return BT_GATT_ITER_CONTINUE;
}

static uint8_t discover_func(struct bt_conn *conn,
			     const struct bt_gatt_attr *attr,
			     struct bt_gatt_discover_params *params)
{
	int err;

	/* This branch is only triggered if attr is NULL */
	/* i.e failed to discover the attribute defined in params */
	if (!attr) {
		LOG_WRN("Discover complete");
		/* Reset discover_params */
		(void)memset(params, 0, sizeof(*params));
		return BT_GATT_ITER_STOP;
	}

	printk("[ATTRIBUTE] handle %u\n", attr->handle);

	/* Handle discovery of Primary Service: NUS, service0010 */
	if (!bt_uuid_cmp(discover_params.uuid, BT_UUID_NUS)) {

		/* Now try to to discover the NUS RX attribute, char0011 */	
		memcpy(&nus_uuid, BT_UUID_NUS_RX, sizeof(nus_uuid));
		discover_params.uuid = &nus_uuid.uuid;
		discover_params.start_handle = attr->handle + 1;  
		discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

		err = bt_gatt_discover(conn, &discover_params);
		if (err) {
			LOG_ERR("Discover failed (err %d)", err);
		}

	/* Handle discovery of NUS RX Service, service0010/char0011 */ 
	} else if (!bt_uuid_cmp(discover_params.uuid, BT_UUID_NUS_RX)) {

		/* Now try to discover the CCC for NUS RX */
		memcpy(&discover_uuid, BT_UUID_GATT_CCC, sizeof(discover_uuid));
		discover_params.uuid = &discover_uuid.uuid;
		discover_params.start_handle = attr->handle + 2;
		discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;

		/* Value handle represents the value/char. the CCC corresponds to */
		subscribe_params.value_handle = bt_gatt_attr_value_handle(attr);

		err = bt_gatt_discover(conn, &discover_params);
		if (err) {
			LOG_ERR("Discover failed (err %d)", err);
		}

	/* Handle discovery of RX service CCC, service0010/char0011/desc0013 */ 
	} else {
		/* Set the callback function */
		subscribe_params.notify = notify_func;
		subscribe_params.value = BT_GATT_CCC_NOTIFY;
		subscribe_params.ccc_handle = attr->handle;

		/* Attempt to subscribe */
		err = bt_gatt_subscribe(conn, &subscribe_params);
		if (err && err != -EALREADY) {
			LOG_ERR("Subscribe failed (err %d)", err);
		} else {
			printk("[SUBSCRIBED]\n");
		}

		return BT_GATT_ITER_STOP;
	}

	return BT_GATT_ITER_STOP;
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                         struct net_buf_simple *ad)
{
    char addr_str[BT_ADDR_LE_STR_LEN];
	int err;

    if (default_conn) {
        return;
    }

    if (type != BT_GAP_ADV_TYPE_ADV_IND && type != BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
        return;
    }

    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
	printk("Device found: %s (RSSI %d)\n", addr_str, rssi);

	/* Compare the MACs */
	bt_addr_le_t target_addr;
    err = bt_addr_le_from_str(TARGET_ADDR_STR, "random", &target_addr);
	if (bt_addr_le_cmp(&target_addr, addr)) {
		return;
	} 

	/* Stop scanning for devices */
	if (bt_le_scan_stop()) {
		return;
	}

	/* Try to connect */
	err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN,
				BT_LE_CONN_PARAM_DEFAULT, &default_conn);
	if (err) {
		LOG_ERR("Create conn to %s failed (%d)", addr_str, err);
		start_scan();
	}
}

static void start_scan(void)
{
	int err;

	/* Use active scanning and disable duplicate filtering to handle any
	 * devices that might update their advertising data at runtime. */
	struct bt_le_scan_param scan_param = {
		.type       = BT_LE_SCAN_TYPE_ACTIVE,
		.options    = BT_LE_SCAN_OPT_NONE,
		.interval   = BT_GAP_SCAN_FAST_INTERVAL,
		.window     = BT_GAP_SCAN_FAST_WINDOW,
	};

	err = bt_le_scan_start(&scan_param, device_found);
	if (err) {
		LOG_ERR("Scanning failed to start (err %d)", err);
		return;
	}

	printk("Scanning successfully started\n");
}

static void mtu_exchange_cb(struct bt_conn *conn, uint8_t err,
			    struct bt_gatt_exchange_params *params)
{
	printk("%s: MTU exchange %s (%u)\n", __func__,
	       err == 0U ? "successful" : "failed",
	       bt_gatt_get_mtu(conn));
}

static struct bt_gatt_exchange_params mtu_exchange_params = {
	.func = mtu_exchange_cb
};

static int mtu_exchange(struct bt_conn *conn)
{
	int err;

	printk("%s: Current MTU = %u\n", __func__, bt_gatt_get_mtu(conn));

	printk("%s: Exchange MTU...\n", __func__);
	err = bt_gatt_exchange_mtu(conn, &mtu_exchange_params);
	if (err) {
		LOG_ERR("%s: MTU exchange failed (err %d)\n", __func__, err);
	}

	return err;
}

static void connected(struct bt_conn *conn, uint8_t conn_err)
{
	char addr[BT_ADDR_LE_STR_LEN];
	int err;

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (conn_err) {
		LOG_ERR("Failed to connect to %s (%u)", addr, conn_err);

		bt_conn_unref(default_conn);
		default_conn = NULL;

		start_scan();
		return;
	}

	printk("Connected: %s\n", addr);
	(void)mtu_exchange(conn);

	if (conn == default_conn) {
		discover_params.uuid = &nus_uuid.uuid;
		discover_params.func = discover_func;
		discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
		discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
		discover_params.type = BT_GATT_DISCOVER_PRIMARY;

		err = bt_gatt_discover(default_conn, &discover_params);
		if (err) {
			LOG_ERR("Discover failed(err %d)", err);
			return;
		}
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Disconnected: %s (reason 0x%02x)\n", addr, reason);

	if (default_conn != conn) {
		return;
	}

	bt_conn_unref(default_conn);
	default_conn = NULL;

	printk("Restarting scan");
	start_scan();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};


void dlt_nus_central_thread(void)
{
    /* Initialise the UART peripheral and register Link with DLT driver */
    k_tid_t link_tid = k_current_get();
    dlt_link_register(NRF_NUS, link_tid);

	int err;
	err = bt_enable(NULL);

	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return;
	}

	start_scan();

	nus_packet_t packet;
	while (true) {
		if (!k_msgq_get(&nus_msgq, &packet, K_FOREVER)) {
			dlt_submit(NRF_NUS, packet.data, packet.length, true);
		}
	}
	return;
}

/* Register the thread */
K_THREAD_DEFINE(dlt_nus_central, DLT_NUS_CENTRAL_STACKSIZE,
			    dlt_nus_central_thread, NULL, NULL, NULL,
				DLT_NUS_CENTRAL_PRIORITY, 0, 0);