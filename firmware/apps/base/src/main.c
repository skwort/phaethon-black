#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/usb/usb_device.h>
#include <pb_encode.h>
#include <pb_decode.h>

#include "dlt_api.h"
#include "dlt_endpoints.h"
#include "phaethon.pb.h"

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

    uint8_t rx_data[DLT_MAX_DATA_LEN] = {0};
    uint8_t resp_len = 0;
    uint8_t msg_type = 0;

    while (true) {

        /* Check the PI UART Link for data */
        resp_len = dlt_read(PI_UART, &msg_type, rx_data, DLT_MAX_DATA_LEN,
                            K_NO_WAIT);
        if (!resp_len) {
            k_sleep(K_MSEC(5));
            continue;
        }

        /* Decode message */
        bool status;

        /* Allocate space for the decoded message. */
        ADSBData message = ADSBData_init_zero;

        /* Create a stream that reads from the buffer. */
        pb_istream_t stream = pb_istream_from_buffer(rx_data, resp_len);

        /* Now we are ready to decode the message. */
        status = pb_decode(&stream, ADSBData_fields, &message);

        /* Check for errors */
        if (status) {
            /* Print the data contained in the message. */
            LOG_INF("hex: %s", (char *)message.hex);
            LOG_INF("flight: %s", message.flight);
            LOG_INF("lat: %f", (double)message.lat);
            LOG_INF("lon: %f", (double)message.lon);
            LOG_INF("alt: %d", message.altitude);
            LOG_INF("speed: %d", message.speed);
            LOG_INF("track: %d", message.track);
        } else {
            LOG_ERR("Decoding failed: %s\n", PB_GET_ERROR(&stream));
        }

        k_sleep(K_MSEC(5));
    }

    return 0;
}
