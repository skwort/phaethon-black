#include "zephyr/logging/log_core.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <pb_encode.h>
#include <pb_decode.h>

#ifdef CONFIG_SHELL_BACKEND_RTT
#include <zephyr/shell/shell_rtt.h>
#else
#include <zephyr/shell/shell.h>
#endif

#include "dlt_api.h"
#include "dlt_endpoints.h"
#include "base_bt.h"
#include "base_gps.h"
#include "phaethon.pb.h"

LOG_MODULE_REGISTER(base_main, LOG_LEVEL_INF);

int main(void)
{
    k_tid_t device_tid = k_current_get();
    dlt_interface_init(2);
    dlt_device_register(device_tid);

    /* Connect to the Thingy52 */
    LOG_INF("Connecting to Thingy52");
#ifdef CONFIG_SHELL_BACKEND_RTT
    shell_execute_cmd(shell_backend_rtt_get_ptr(), "blecon -s c8:91:07:19:03:58");
#else
    shell_execute_cmd(shell_backend_uart_get_ptr(), "blecon -s c8:91:07:19:03:58");
#endif

    uint8_t rx_data[DLT_MAX_DATA_LEN] = {0};
    uint8_t tx_buf[DLT_MAX_DATA_LEN] = {0};
    uint8_t resp_len = 0;
    uint8_t msg_type = 0;
    wsu_data_packet pkt; 
    gps_base_data gps;

    while (true) {

        /* Check the PI UART Link for data */
        resp_len = dlt_read(PI_UART, &msg_type, rx_data, DLT_MAX_DATA_LEN,
                            K_NO_WAIT);
        if (resp_len) {

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

            /* Forward message */
            LOG_INF("Forwarding packet to M5.");
            dlt_request(M5_NUS, tx_buf, rx_data, resp_len, true);
        }

        /* Check IMU data */
        if (!base_bt_wsu_data_recv(&pkt, K_NO_WAIT)) {
            /* Print the packet */
            LOG_INF("p %f, r %f, y %f", (double)pkt.pitch,
                    (double)pkt.roll, (double)pkt.yaw);
        }

        /* Check GPS data */
        if (!base_gps_i2c_data_recv(&gps, K_NO_WAIT)) {
                if (gps.good_data) {
                    LOG_INF("lat: %f, lon %f", (double)gps.latitude, (double)gps.longitude);
                } else {
                    LOG_INF("GPS data is bad.");
                }
        }

        k_sleep(K_MSEC(3));
    }

    return 0;
}
