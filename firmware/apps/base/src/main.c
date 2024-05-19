#include "zephyr/logging/log_core.h"
#include <math.h>
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

#define PI 3.14159265358979323846f

/* Filter window for compass */
#define BEARING_FILTER_APERTURE  30.f

// Convert degrees to radians
static inline float degrees_to_radians(float degrees)
{
    return degrees * PI / 180.0f;
}

// Convert radians to degrees
static inline float radians_to_degrees(float radians)
{
    return radians * 180.0f / PI;
}

// Calculate the bearing between two points using the rhumb line technique
static float calculate_rhumb_line_bearing(float lat1, float lon1, float lat2,
                                          float lon2)
{
    
    /* Precalculate repeated values */
    const static float PI_ON_4 = PI / 4.0f;
    const static float TWO_PI = 2.0f * PI;
    
    // Convert latitudes and longitudes from degrees to radians
    float phi1 = degrees_to_radians(lat1);
    float phi2 = degrees_to_radians(lat2);
    float lambda1 = degrees_to_radians(lon1);
    float lambda2 = degrees_to_radians(lon2);

    // Calculate the projected latitude difference (Δψ)
    float delta_psi = logf(tanf(PI_ON_4 + phi2 / 2.0f) / tanf(PI_ON_4 + phi1 / 2.0f));

    // Calculate the difference in longitudes (Δλ)
    float delta_lambda = lambda2 - lambda1;

    // Adjust the difference in longitudes (Δλ) if necessary
    if (fabsf(delta_lambda) > PI) {
        if (delta_lambda > 0) {
            delta_lambda = -(TWO_PI - delta_lambda);
        } else {
            delta_lambda = TWO_PI + delta_lambda;
        }
    }

    // Calculate the bearing (brng)
    float bearing = atan2f(delta_lambda, delta_psi);

    // Convert the bearing from radians to degrees
    bearing = radians_to_degrees(bearing);

    // Normalize the bearing to be within the range [0, 360) degrees
    if (bearing < 0) {
        bearing += 360.0f;
    }

    return bearing;
}

// Check if the calculated bearing is within 30 degrees of the reference bearing
static bool is_within_bearing(float calculated_bearing, float reference_bearing) {
    float difference = fabsf(calculated_bearing - reference_bearing);

    // Adjust the difference to be the smallest angle between the bearings
    if (difference > 180.0f) {
        difference = 360.0f - difference;
    }

    // Return true if the difference is within 30 degrees
    return difference <= BEARING_FILTER_APERTURE;
}

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

    wsu_data_packet wsu; 
    bool wsu_conn = false;

    gps_base_data gps;
    gps.good_data = false;

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
                LOG_INF("Got packet for hex: %s", (char *)message.hex);
                // LOG_INF("flight: %s", message.flight);
                // LOG_INF("lat: %f", (double)message.lat);
                // LOG_INF("lon: %f", (double)message.lon);
                // LOG_INF("alt: %d", message.altitude);
                // LOG_INF("speed: %d", message.speed);
                // LOG_INF("track: %d", message.track);
            } else {
                LOG_ERR("Decoding failed: %s\n", PB_GET_ERROR(&stream));
            }

            /* GPS not bad */
            if (gps.good_data) {
                /* Filter the message */
                float rhumb_bearing = calculate_rhumb_line_bearing(gps.latitude,
                                                                gps.longitude,
                                                                message.lat,
                                                                message.lon);

                bool allow = is_within_bearing(rhumb_bearing, wsu.yaw);
                if (allow && wsu_conn) {
                    /* Forward message */
                    LOG_INF("Forwarding packet to M5.");
                    dlt_request(M5_NUS, tx_buf, rx_data, resp_len, true);
                } else {
                    LOG_INF("Ignoring packet. Not in heading.");
                }
            }
        }

        /* Check IMU data */
        if (!base_bt_wsu_data_recv(&wsu, K_NO_WAIT)) {
            /* Print the packet */
            // LOG_INF("p %f, r %f, y %f", (double)wsu.pitch,
            //         (double)wsu.roll, (double)wsu.yaw);
            LOG_INF("Yaw: %.02f", (double)wsu.yaw);
            wsu_conn = true;
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
