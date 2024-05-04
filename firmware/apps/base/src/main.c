#include <stdio.h>
#include <string.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/usb/usb_device.h>


LOG_MODULE_REGISTER(base_main);

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS 1000

/* get uart device reference */
const struct device *uart = DEVICE_DT_GET(DT_NODELABEL(uart0));

int main(void)
{

    if (!device_is_ready(uart)) {
        printk("UART device not ready\r\n");
        return 1;
    }

    return 0;
}
