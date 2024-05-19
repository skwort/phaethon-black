#include "zephyr/logging/log_core.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <pb_decode.h>
#include <zephyr/device.h>
#include <zephyr/display/cfb.h>

#include "dlt_api.h"
#include "dlt_endpoints.h"
#include "phaethon.pb.h"

LOG_MODULE_REGISTER(m5_main, LOG_LEVEL_INF);

bool init_display(const struct device *dev)
{
	uint16_t x_res;
	uint16_t y_res;
	uint16_t rows;
	uint8_t ppt;
	uint8_t font_width;
	uint8_t font_height;

	if (!device_is_ready(dev)) {
		printf("Device %s not ready\n", dev->name);
		return false;
	}

	if (display_set_pixel_format(dev, PIXEL_FORMAT_MONO01) != 0) {
		if (display_set_pixel_format(dev, PIXEL_FORMAT_MONO10) != 0) {
			printf("Failed to set required pixel format");
			return false;
		}
	}

	printf("Initialized %s\n", dev->name);

	if (cfb_framebuffer_init(dev)) {
		printf("Framebuffer initialization failed!\n");
		return false;
	}

	cfb_framebuffer_clear(dev, true);

	//display_blanking_off(dev);

	x_res = cfb_get_display_parameter(dev, CFB_DISPLAY_WIDTH);
	y_res = cfb_get_display_parameter(dev, CFB_DISPLAY_HEIGH);
	rows = cfb_get_display_parameter(dev, CFB_DISPLAY_ROWS);
	ppt = cfb_get_display_parameter(dev, CFB_DISPLAY_PPT);

    for (int idx = 0; idx < 42; idx++) {
		if (cfb_get_font_size(dev, idx, &font_width, &font_height)) {
			break;
		}
		cfb_framebuffer_set_font(dev, idx);
		printf("font width %d, font height %d\n",
		       font_width, font_height);
        break;
	}

	printf("x_res %d, y_res %d, ppt %d, rows %d, cols %d\n",
	       x_res,
	       y_res,
	       ppt,
	       rows,
	       cfb_get_display_parameter(dev, CFB_DISPLAY_COLS));

	cfb_set_kerning(dev, 3);

    return true;
}

int main(void)
{
    /* Setup display */
    const struct device *dev;
	dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    init_display(dev);

    k_tid_t device_tid = k_current_get();
    dlt_interface_init(1);

    dlt_device_register(device_tid);

    uint8_t rx_data[DLT_MAX_DATA_LEN] = {0};
    uint8_t resp_len = 0;
    uint8_t msg_type = 0;
    int64_t last_packet = 0;
    int64_t now = 0;

    LOG_INF("Starting main loop");

    while (true) {

        /* Get the system tick */
        now = k_uptime_get();

        /* Check the PI UART Link for data */
        resp_len = dlt_read(NRF_NUS, &msg_type, rx_data, DLT_MAX_DATA_LEN,
                            K_NO_WAIT);
        if (resp_len) {
            LOG_INF("Message received.");
            last_packet = now;

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

                /* Print to the display */
                cfb_framebuffer_clear(dev, false);
                char pbuf[40]; // Buffer for formatting strings
                int err = 0;
                int y_pos = 0; // Starting y-coordinate

                // Print each data field on a separate line, incrementing the y-coordinate each time
                sprintf(pbuf, "%s", message.flight);
                err = cfb_print(dev, pbuf, 2, y_pos);
                if (err) {
                    printk("Failed to print flight\n");
                }
                y_pos += 15; // Adjust y-coordinate for the next line

                sprintf(pbuf, "%s", message.hex);
                err = cfb_print(dev, pbuf, 2, y_pos);
                if (err) {
                    printk("Failed to print hex\n");
                }
                y_pos += 15; // Adjust y-coordinate for the next line

                sprintf(pbuf, "Lat%.02f", (double)message.lat);
                err = cfb_print(dev, pbuf, 2, y_pos);
                if (err) {
                    printk("Failed to print latitude\n");
                }
                y_pos += 15; // Adjust y-coordinate for the next line

                sprintf(pbuf, "Lon%.02f", (double)message.lon);
                err = cfb_print(dev, pbuf, 2, y_pos);
                if (err) {
                    printk("Failed to print longitude\n");
                }
                y_pos += 15; // Adjust y-coordinate for the next line

                // Continue printing additional data fields in a similar fashion

                cfb_framebuffer_finalize(dev);

            } else {
                LOG_ERR("Decoding failed: %s\n", PB_GET_ERROR(&stream));
            }

        } else if (!resp_len && (now - last_packet > 5000)) {
            printk("No data received in 5 seconds\n");
            cfb_framebuffer_clear(dev, true);
        }

        k_sleep(K_MSEC(3));
    }

    return 0;
}
