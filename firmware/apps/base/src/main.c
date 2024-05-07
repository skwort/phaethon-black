#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/usb/usb_device.h>

#include "dlt_api.h"
#include "dlt_endpoints.h"
#include "zephyr/kernel/thread.h"
#include "zephyr/sys/printk.h"

LOG_MODULE_REGISTER(base_main);

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS 1000

/* get uart device reference */
const struct device *uart = DEVICE_DT_GET(DT_NODELABEL(uart0));

int main(void)
{
    k_tid_t device_tid = k_current_get();
    dlt_interface_init(1);
    dlt_device_register(device_tid);

    if (!device_is_ready(uart)) {
        printk("UART device not ready\r\n");
        return 1;
    }


    uint8_t data[] = "david";
    uint8_t packet[50] = {0};
    uint8_t rpacket[50] = {0};
    bool resp_pending = false;
    while (true) {
        if (!resp_pending) {
            /* Req */
            LOG_INF("REQUEST");
            dlt_request(PI_UART, packet, data, 5, true);
            resp_pending = true;
        } else if (resp_pending) {
            /* Get resp. */
            uint8_t resp_len = dlt_read(PI_UART, rpacket, 50, K_FOREVER);
            if (resp_len) { 
                LOG_INF("GOT RESPONSE");
                printk("resp: %s\n", rpacket);
                resp_pending = false;
            }
            LOG_INF("NO RESPONSE");
        }
        k_sleep(K_MSEC(10));
    }

    return 0;
}
