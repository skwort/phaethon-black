#ifndef BASE_GPS_H_
#define BASE_GPS_H_

#include <zephyr/kernel.h>

/* Struct to be sent over thread */
typedef struct {
    bool good_data; // if status flag is A = true, if V = false
    float latitude;  // decimal degrees
    float longitude;
} gps_base_data;

/* Prototypes */
int base_gps_i2c_data_recv(gps_base_data *gps_struct, k_timeout_t timeout);

#endif