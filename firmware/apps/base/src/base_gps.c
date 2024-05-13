/*
 * flynnmkelly
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "base_gps.h"

LOG_MODULE_REGISTER(gps_module, LOG_LEVEL_DBG);

/* Zephyr Handlers */
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
#define I2C_DEVICE_NODE DT_NODELABEL(i2c0) 

/* Thread Parameters */
#define T_BASE_GPS_STACKSIZE 1024 * 4
#define T_BASE_GPS_PRIORITY  7

/* GPS i2C */
#define GPS_I2C_ADDR 0x10 // 7-bit unshifted default I2C Address
#define MAX_GPS_PACKET_SIZE 255
static uint8_t gps_data[MAX_GPS_PACKET_SIZE];

/* Config */
static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);
static const struct device *i2c_dev;
uint32_t i2c_cfg = I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_CONTROLLER;

/* NMEA Parsing */
#define PMTK_FULL_COLD_START "$PMTK103*30<CR><LF>"  
#define MAX_NMEA_SENTENCE_SIZE 80
uint8_t gnrmc_sentence[MAX_NMEA_SENTENCE_SIZE];
gps_base_data gps_data_struct;

/* Define the message queue for comms with control thread */
K_MSGQ_DEFINE(gps_base_msgq, sizeof(gps_base_data), 10, 4);

/* *** FUNCTIONS TO BE CALLED IN main.c/CONTROsL THREAD *** */

/* Receive a gps data struct from the queue */
int base_gps_i2c_data_recv(gps_base_data *gps_struct, k_timeout_t timeout) {
    
    return k_msgq_get(&gps_base_msgq, gps_struct, timeout);

}


/* *** FUNCTIONS WHICH RUN IN THREAD *** */
bool gps_init(const struct device *dev) {
    if (!device_is_ready(dev)) {
        LOG_ERR("I2C device is not ready");
        return false;
    }

	/* Verify i2c_configure() */
	if (i2c_configure(dev, i2c_cfg)) {
		LOG_ERR("I2C config failed\n");
		return false;
	}

    uint8_t dummy = 0x00;
    int ret = i2c_write(dev, &dummy, 1, GPS_I2C_ADDR);
    if (ret) {
        LOG_ERR("GPS module not responding at address 0x%02X", GPS_I2C_ADDR);
        return false;
    }
    LOG_INF("GPS module found at address 0x%02X!", GPS_I2C_ADDR);
    
	k_sleep(K_MSEC(10));

	// Attempt to read a byte to ensure the device is responding
    uint8_t response;
    ret = i2c_read(dev, &response, 1, GPS_I2C_ADDR);
    if (ret) {
        LOG_ERR("Failed to read from GPS module at address 0x%02X", GPS_I2C_ADDR);
        return false;
    }

    LOG_INF("GPS module found and responding at address 0x%02X!", GPS_I2C_ADDR);
    return true;
}

static int send_full_cold_start_command(const struct device *i2c_dev) {
    const char *cmd = PMTK_FULL_COLD_START;
    size_t len = strlen(cmd);
    int ret = i2c_write(i2c_dev, cmd, len, GPS_I2C_ADDR);
    if (ret != 0) {
        LOG_ERR("Failed to send full cold start command: %d", ret);
    } else {
        LOG_INF("Full cold start command sent successfully.");
    }
    return ret;
}

/**
 * This function reads an NMEA packet from the GPS module via I2C communication.
 * It reads the packet in chunks of maximum buffer size and appends them to the provided buffer.
 */
static int read_nmea_packet(uint8_t *buffer, size_t size)
{
    int ret;
    size_t total_read = 0;
    const size_t read_size = 255;  // Max buffer size to read at once

    while (total_read < size) {
        ret = i2c_burst_read(i2c_dev, GPS_I2C_ADDR, 0x00, buffer + total_read, read_size);
        if (ret) {
            LOG_ERR("I2C read failed with error %d", ret);
            return ret;
        }
        total_read += read_size;
        k_sleep(K_MSEC(2)); // Short sleep to allow buffer refresh
    }

    buffer[size - 1] = '\0';  // Ensure null-termination

    return 0;
}


/* Reads and Logs NMEA Packets */
static void check_gps_data(void) {
    int ret = read_nmea_packet(gps_data, MAX_GPS_PACKET_SIZE);
    if (ret == 0) {
        LOG_INF("GPS data processed successfully.");
        // Here you can add further processing or handling of the GPS data if needed
        
        // OUTPUT RAW VALUES from TitanX1 Buffer
        //printk("GPS Data: %s\n", gps_data);

    } else {
        LOG_ERR("Failed to read GPS data.");
    }
}

/*
 * Function to find and log the GNRMC sentence from the GPS data buffer
 * and store it in a global buffer for further processing.
 */
static void extract_gnrmc(const uint8_t *buffer) {
    // Pointer to the start of the GNRMC sentence within the buffer
    const char *gnrmc_start = strstr((const char *)buffer, "$GNRMC");
    if (gnrmc_start != NULL) {
        // Find the end of the GNRMC sentence, marked by the newline character
        const char *gnrmc_end = strchr(gnrmc_start, '\n');
        if (gnrmc_end != NULL) {
            // Calculate the length of the GNRMC sentence
            size_t gnrmc_length = gnrmc_end - gnrmc_start;
            if (gnrmc_length > 1 && gnrmc_length < MAX_NMEA_SENTENCE_SIZE) {
                // Copy the sentence into the global buffer, ensuring not to overflow
                strncpy((char *)gnrmc_sentence, gnrmc_start, gnrmc_length);
                gnrmc_sentence[gnrmc_length] = '\0';  // Null-terminate the string

                // Log the GNRMC sentence
                LOG_INF("GNRMC Sentence stored: %s", gnrmc_sentence);

            } else {
                LOG_ERR("GNRMC sentence length is too long or too short");
            }
        } else {
            LOG_ERR("GNRMC sentence not properly terminated");
        }
    } else {
        LOG_ERR("GNRMC sentence not found in buffer");
    }
}

/*
 * Function to convert latitude and longitude from NMEA format (ddmm.mmmm) to decimal degrees
 */
static float nmea_to_decimal(float nmea_value, char direction) {
    int degrees = (int)(nmea_value / 100);
    float minutes = nmea_value - (degrees * 100);
    float decimal_degrees = degrees + (minutes / 60);
    if (direction == 'S' || direction == 'W') {  // For Southern Hemisphere or Western Hemisphere
        decimal_degrees = -decimal_degrees;
    }
    return decimal_degrees;
}

/*
 * Function to parse GNRMC sentence and update the global GPS data struct
 * $GNRMC,234042.000,A,2729.7905,S,15259.7095,E,1.00,262.90,120524,,,A*64
 */
void parse_gnrmc(const uint8_t *gnrmc_sentence) {
    char status = 'V', lat_dir = 'S', long_dir = 'E';
    float latitude = 0.0, longitude = 0.0;
    char *token;
    int field_count = 0;

    // Create a copy of the sentence for strtok manipulation
    char sentence_copy[100];
    strncpy(sentence_copy, (const char *)gnrmc_sentence, 99);
    sentence_copy[99] = '\0';

    token = strtok(sentence_copy, ",");

    while (token != NULL) {
        field_count++;
        switch (field_count) {
            case 3:  // Status
                status = token[0];
                break;
            case 4:  // Latitude
                if (token[0] != '\0') latitude = atof(token);
                break;
            case 5:  // Latitude direction
                if (token[0] != '\0') lat_dir = token[0];
                break;
            case 6:  // Longitude
                if (token[0] != '\0') longitude = atof(token);
                break;
            case 7:  // Longitude direction
                if (token[0] != '\0') long_dir = token[0];
                break;
        }
        token = strtok(NULL, ",");
    }

    // Set the struct fields
    gps_data_struct.good_data = (status == 'A'); // works
    gps_data_struct.latitude = nmea_to_decimal(latitude, lat_dir);
    gps_data_struct.longitude = nmea_to_decimal(longitude, long_dir);
}

/* Thread which sends relevant GPS data to main */
void base_gps_thread(void)
{

    if (!device_is_ready(uart_dev)) {
        LOG_ERR("UART device not found!\n");
        return;
    }

    i2c_dev = DEVICE_DT_GET(I2C_DEVICE_NODE);
    if (!gps_init(i2c_dev)) {
        LOG_ERR("GPS initialization failed!\n");
        return;
    }
	k_sleep(K_MSEC(10));

    // Sending the full cold start command
    if (send_full_cold_start_command(i2c_dev) != 0) {
        LOG_ERR("Failed to reset GPS module.\n");
    } else { // wsa successful
        //printk("GPS module reset successfully.\n");
    }
    k_sleep(K_MSEC(10));

    while (1) {
        
		// poll evert 500mss
        check_gps_data();
        extract_gnrmc(gps_data);

        // parse data into struct
        parse_gnrmc(gnrmc_sentence);

        // send data via msgqueue
        k_msgq_put(&gps_base_msgq, &gps_data_struct, K_MSEC(3000));
        
        // delay for relevant time (let titanX1 buffer fill)
        k_sleep(K_MSEC(2000));

    }
}


K_THREAD_DEFINE(base_gps, T_BASE_GPS_STACKSIZE, base_gps_thread, NULL, NULL, NULL, T_BASE_GPS_PRIORITY, 0, 0);


/* SOME MAIN CODE TO TEST THE FUNCTIONALITY */

/*
 * flynnmkelly
 */

// /* TESTING RECEIVING DATA VIA THE MESSAGE QUEUE */
// #include <zephyr/kernel.h>
// #include <zephyr/logging/log.h>

// #include "base_gps.h"

// LOG_MODULE_REGISTER(main);

// int main(void)
// {
//     gps_base_data gps_data_main;
//     k_timeout_t timeout = K_MSEC(5000);  // 5-second timeout for non-blocking behavior
//     LOG_INF("Starting main thread.");

//     while (true) {
//         int ret = base_gps_i2c_data_recv(&gps_data_main, timeout);
//         if (ret == 0) {
//             // char formatted_lat[20], formatted_lon[20];
//             // // Format latitude and longitude with 6 decimal places
//             // snprintf(formatted_lat, sizeof(formatted_lat), "%.6f", gps_data_main.latitude);
//             // snprintf(formatted_lon, sizeof(formatted_lon), "%.6f", gps_data_main.longitude);

//             LOG_INF("Received GPS data: %d, Latitude: %d, Longitude: %d", gps_data_main.good_data, (int)gps_data_main.latitude, (int)gps_data_main.longitude);
//         } else {
//             LOG_ERR("Failed to receive GPS data: %d", ret);
//         }
//     }

//     return 0;
// }