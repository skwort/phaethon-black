#include <zephyr/types.h>
#include <stddef.h>
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/sys/byteorder.h>

#include <zephyr/bluetooth/services/nus.h>

static void start_scan(void);

/* CHANGE ADDRESS AS REQUIRED - find in peripheral code */
//#define TARGET_ADDR_STR "FF:A2:86:CD:34:66" // address of peripheral - flynns nrf board
#define TARGET_ADDR_STR "78:21:84:8D:E1:38" // m5stackcore2 board
static bt_addr_le_t target_addr;

#define BT_UUID_NUS BT_UUID_DECLARE_128(0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E)
#define BT_UUID_NUS_RX BT_UUID_DECLARE_128(0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x6E)
#define BT_UUID_NUS_TX BT_UUID_DECLARE_128(0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x03, 0x00, 0x40, 0x6E)

static struct bt_conn *default_conn;

// NUS / GATT DISCOVERY
static struct bt_uuid_128 nus_uuid = BT_UUID_INIT_128(BT_UUID_NUS_SRV_VAL);
static struct bt_gatt_discover_params discover_params;
static struct bt_gatt_subscribe_params subscribe_params;


static uint8_t notify_func(struct bt_conn *conn,
			   struct bt_gatt_subscribe_params *params,
			   const void *data, uint16_t length)
{ 	
	printk("Got to notify_func\n");
	
	if (!data) {
		printk("[UNSUBSCRIBED]\n");
		params->value_handle = 0U;
		return BT_GATT_ITER_STOP;
	}

	printk("[NOTIFICATION] data %p length %u\n", data, length);

	return BT_GATT_ITER_CONTINUE;
}

/* Function that parses NUS protocol */
static uint8_t discover_func(struct bt_conn *conn,
			     const struct bt_gatt_attr *attr,
			     struct bt_gatt_discover_params *params)
{
    int err;

	// Checks end of discovery process - prints done
    if (!attr) {
        printk("Discover complete\n");
        (void)memset(params, 0, sizeof(*params));
		printk("GOt past memset\n");
        return BT_GATT_ITER_STOP;
    }
 
	// Else handles dicovery process

	printk("Got to handle discovery\n");

    printk("[ATTRIBUTE] handle %u\n", attr->handle);

    if (!bt_uuid_cmp(discover_params.uuid, BT_UUID_NUS)) {
        // Discover NUS Service
        printk("BT_UUID_NUS found\n");
		memcpy(&nus_uuid, BT_UUID_NUS_RX, sizeof(nus_uuid));
		
		

        discover_params.uuid = &nus_uuid.uuid;
        discover_params.start_handle = attr->handle + 1;
        discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

        err = bt_gatt_discover(conn, &discover_params);
        if (err) {
            printk("Discover failed (err %d)\n", err);
        }

		printk("Completed gatt_discover NUS attribute\n");

	// ********* TX / RX ASSIGNMENT CLARIFY *********
    } else if (!bt_uuid_cmp(discover_params.uuid, BT_UUID_NUS_RX)) {
        // Discover NUS RX Characteristic
		printk("BT_UUID_NUS RX found\n");
        memcpy(&nus_uuid, BT_UUID_NUS_TX, sizeof(nus_uuid)); // WHY TX?
        discover_params.uuid = &nus_uuid.uuid;
        discover_params.start_handle = attr->handle + 1;
        discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
        // subscribe_params.value_handle = bt_gatt_attr_value_handle(attr);

        err = bt_gatt_discover(conn, &discover_params);
        if (err) {
            printk("Discover failed (err %d)\n", err);
        }

	} else if (!bt_uuid_cmp(discover_params.uuid, BT_UUID_NUS_TX)) {
        // // Discover NUS TX Characteristic
		// printk("BT_UUID_NUS TX found\n");
		// // here could be error
        // memcpy(&nus_uuid, BT_UUID_GATT_CCC, sizeof(nus_uuid));
        // discover_params.uuid = &nus_uuid.uuid;
        // discover_params.start_handle = attr->handle + 1;
        // discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
        // subscribe_params.value_handle = bt_gatt_attr_value_handle(attr);

        // err = bt_gatt_discover(conn, &discover_params);
        // if (err) {
        //     printk("Discover failed (err %d)\n", err);
        // }	

		printk("BT_UUID_NUS_TX found\n");
        struct bt_uuid_16 uuid = BT_UUID_INIT_16(0);
		memcpy(&uuid, BT_UUID_GATT_CCC, sizeof(uuid));
		discover_params.uuid = &nus_uuid.uuid;
		discover_params.start_handle = attr->handle + 2;
		discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
		subscribe_params.value_handle =  bt_gatt_attr_value_handle(attr);

		err = bt_gatt_discover(conn, &discover_params);
		if (err) {
			printk("Discover failed (err %d)\n", err);
		}

    } else {
        // Got here
		printk("Got to subscribe stuff \n");

        subscribe_params.notify = notify_func;
        subscribe_params.value = BT_GATT_CCC_NOTIFY;
        subscribe_params.ccc_handle = attr->handle;

        err = bt_gatt_subscribe(conn, &subscribe_params);
        if (err && err != -EALREADY) {
            printk("Subscribe failed (err %d)\n", err);
        } else {
            printk("[SUBSCRIBED]\n");
        }

        return BT_GATT_ITER_STOP;
    }

	printk("about to return GATT_ITER\n");

    return BT_GATT_ITER_CONTINUE;

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

	/* connect only to devices in close proximity */
	if (rssi < -50) {
		return;
	}

	if (bt_le_scan_stop()) {
		return;
	}

	err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN,
				BT_LE_CONN_PARAM_DEFAULT, &default_conn);
	if (err) {
		printk("Create conn to %s failed (%d)\n", addr_str, err);
		start_scan();
	}

	/* CUSTOM ADDRESS STUFF */
    // if (!bt_addr_le_cmp(addr, &target_addr)) {
    //     printk("Target device found, RSSI %d\n", rssi);

    //     if (rssi < -50) {
    //         return;
    //     }

    //     if (bt_le_scan_stop() == 0) {
    //         if (bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN,
    //                               BT_LE_CONN_PARAM_DEFAULT, &default_conn)) {
    //             printk("Failed to initiate connection\n");
    //         }
    //     }
    // }
}

static void start_scan(void)
{
    int err;

    // Convert target address from string
    // if (bt_addr_le_from_str(TARGET_ADDR_STR, "random", &target_addr)) {
    //     printk("Invalid target Bluetooth address!\n");
    //     return;
    // }

    // Start scanning
    err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
    if (err) {
        printk("Scanning failed to start (err %d)\n", err);
    } else {
        printk("Scanning successfully started\n");
    }

    printk("inside scanning\n");
}

static void connected(struct bt_conn *conn, uint8_t err)
{   
    printk("Starting connection\n");
    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr_str, sizeof(addr_str));

    if (err) {
        printk("Failed to connect to %s (err %u)\n", addr_str, err);
        bt_conn_unref(default_conn);
        default_conn = NULL;
        start_scan();
        return;
    }

    printk("Connected to %s\n", addr_str);

	if (conn == default_conn) {
		memcpy(&nus_uuid, BT_UUID_NUS, sizeof(nus_uuid));
		discover_params.uuid = &nus_uuid.uuid;
		discover_params.func = discover_func;
		discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
		discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
		discover_params.type = BT_GATT_DISCOVER_PRIMARY;

		err = bt_gatt_discover(default_conn, &discover_params);
		if (err) {
			printk("Discover failed(err %d)\n", err);
			return;
		}

		// gets here
		//printk("got through bt_gatt_discover \n");
	}

	// Code is getting to here without breaking, what happens next??
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr_str, sizeof(addr_str));

    printk("Disconnected from %s (reason %02x)\n", addr_str, reason);
    bt_conn_unref(default_conn);
    default_conn = NULL;

    start_scan();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

int main(void)
{
    int err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return 0;
    }

    printk("Bluetooth initialized\n");
    
	start_scan();

    return 0;
}

