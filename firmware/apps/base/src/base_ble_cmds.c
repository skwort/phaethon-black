#include <stdio.h>
#include <string.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

#include "base_bt.h"

LOG_MODULE_REGISTER(ble_cmds_module);

static int cmd_base_ble_con_usage(const struct shell *sh, size_t argc,
                                 char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    shell_print(sh, "Usage:\n"
                    "    blecon -s <MAC ADDRESS>\n"
                    "    blecon -p\n");
    return 0;
}

static int cmd_base_ble_scan_usage(const struct shell *sh, size_t argc,
                                  char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    shell_print(sh, "Usage:\n"
                    "    blescan [-s|-p] -f <MAC ADDRESS>\n"
                    "    The -f option is optional.");
    return 0;
}

static int cmd_base_ble_con(const struct shell *sh, size_t argc, char **argv)
{
    if (argc <= 1 || argc > 3) {
        LOG_ERR("Invalid command.");
        cmd_base_ble_con_usage(sh, 0, NULL);
        return 1;
    }

    /* Validate and process command */
    if (!strcmp(argv[1], "-s")) {
        if (argc != 3) {
            cmd_base_ble_con_usage(sh, 0, NULL);
            return 1;
        }

        /* Process the MAC address */
        bt_addr_le_t filter_addr;
        int err = bt_addr_le_from_str(argv[2], "random", &filter_addr);
        if (err) {
            shell_print(sh, "Error. MAC address '%s' is malformed. Err %d",
                        argv[2], err);
            return 1;
        }

        LOG_INF("Connecting to WSU at %s", argv[2]);

        /* Send the base_bt conn start cmd */
        base_bt_cmd_t cmd = {
            .cmd_type = BASE_BT_CONN_START,
            .filter = true,
            .addr = filter_addr,
        };

        base_bt_cmd_send(&cmd, K_FOREVER);

    } else if (!strcmp(argv[1], "-p")) {
        if (argc != 2) {
            cmd_base_ble_con_usage(sh, 0, NULL);
            return 1;
        }

        /* Send the stop cmd */
        base_bt_cmd_t cmd = {
            .cmd_type = BASE_BT_CONN_STOP,
            .filter = false,
            .addr = (bt_addr_le_t)*BT_ADDR_LE_NONE,
        };
        base_bt_cmd_send(&cmd, K_FOREVER);

    } else {
        LOG_ERR("Invalid option.");
        cmd_base_ble_con_usage(sh, 0, NULL);
        return 1;
    }

    return 0;
}

static int cmd_base_ble_scan(const struct shell *sh, size_t argc, char **argv)
{
    if (argc <= 1 || argc > 4 || argc == 3) {
        LOG_ERR("Invalid command.");
        cmd_base_ble_scan_usage(sh, 0, NULL);
        return 1;
    }

    /* Get the filter if it exists */
    bool use_filter = false;
    bt_addr_le_t filter_addr;
    if (argc == 4 && !strcmp(argv[2], "-f")) {
        /* Attempt parse the MAC address*/
        if (bt_addr_le_from_str(argv[3], "random", &filter_addr)) {
            shell_print(sh, "Error. MAC address '%s' is malformed.", argv[3]);
            return 1;
        }
        use_filter = true;
    }

    /* Validate and process command */
    if (!strcmp(argv[1], "-s")) {
        /* Send the start command */
        base_bt_cmd_t cmd = {
            .cmd_type = BASE_BT_SCAN_START,
            .filter = use_filter,
            .addr = use_filter ? filter_addr : (bt_addr_le_t)*BT_ADDR_LE_NONE,
        };

        base_bt_cmd_send(&cmd, K_FOREVER);

    } else if (!strcmp(argv[1], "-p")) {
        /* Send the stop command */
        base_bt_cmd_t cmd = {
            .cmd_type = BASE_BT_SCAN_STOP,
            .filter = use_filter,
            .addr = use_filter ? filter_addr : (bt_addr_le_t)*BT_ADDR_LE_NONE,
        };

        base_bt_cmd_send(&cmd, K_FOREVER);

    } else {
        LOG_ERR("Invalid argument.");
        cmd_base_ble_scan_usage(sh, 0, NULL);
        return 1;
    }

    return 0;
}

SHELL_CMD_REGISTER(blecon, NULL, "Connect to the WSU.", cmd_base_ble_con);
SHELL_CMD_REGISTER(blescan, NULL, "Scan for BLE devices.", cmd_base_ble_scan);