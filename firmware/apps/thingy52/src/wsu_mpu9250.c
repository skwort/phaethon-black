/**
 * @file wsu_mpu9250.c
 * @brief MPU9250 Data Capture Thread
 *
 * This file contains the data reading and data processing logic for generating
 * pitch, roll and yaw values using the Thingy52's IMU (MPU9250). Some of the
 * sensor data processing code is adapted from kriswiner's MPU9250 library.
 *
 * @author Sam Kwort
 * @date  28/04/2024
 *
 * @see https://github.com/kriswiner/MPU9250/tree/master
 */

#include "filter.h"
#include "wsu_msg_api.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <math.h>

/* Setup logging */
LOG_MODULE_REGISTER(wsu_mpu9250_module, LOG_LEVEL_ERR);

/* Thread parameters */
#define T_MPU9250_STACKSIZE 1024
#define T_MPU9250_PRIORITY  7

/* Devicetree nodes*/
#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

#define SW0_NODE	DT_ALIAS(sw0)
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios,
							      {0});
static struct gpio_callback button_cb_data;

/* Define calibration semaphore with no availability */
K_SEM_DEFINE(calibration_sem, 0, 1);

/* Sensor resolutions */
#define A_RES 1.0f/9.806650f  // Convert from m/s/s to g's
#define G_RES 1
#define M_RES 1000.0f         // Zephyr driver is in G, filter needs mG

/* Default soft and hard iron calibration values for magnetometer */
float mag_bias[3] = {0.425760f, -2.013490f, -9.738627f};
float mag_scale[3] = {1.570795f, 0.941597f, 0.768430f};

/* Filter error constants */
#define PI 3.14159265358979323846f
const float GYRO_MEAS_ERR    = PI * (40.0f / 180.0f);   //  error in rads/s
const float GYRO_MEAS_DRIFT  = PI * (0.0f  / 180.0f);   //  drift in rad/s/s

/* Declination for Brisbane is 11.12 degress. */
#define DECLINATION 11.12f


void button_pressed(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
    k_sem_give(&calibration_sem);
}

static void mpu9250_get_mag_sample(const struct device *dev, float *m)
{
	struct sensor_value mag[3];

    /* Fetch the sample */
    if (sensor_sample_fetch(dev) < 0) {
        LOG_ERR("Sensor sample update error");
        return;
    }

    /* Read the magnetometer */
    if (sensor_channel_get(dev, SENSOR_CHAN_MAGN_XYZ, mag) < 0) {
        LOG_ERR("Cannot read magnetometer");
        return;
    }

    /* Don't scale values here */
    m[0] = sensor_value_to_float(&mag[0]);
    m[1] = sensor_value_to_float(&mag[1]);
    m[2] = sensor_value_to_float(&mag[2]);
}

void mpu9250_mag_cal(const struct device *dev, float *bias_dest, float *scale_dest) 
{
    uint16_t ii = 0, sample_count = 0;
    float temp_scale[3] = {0, 0, 0};
    float mag_max[3] = {-20.0f, -20.0f, -20.0f};
    float mag_min[3] = {20.0f, 20.0f, 20.0f};
    float mag_temp[3] = {0, 0, 0};

    printk("Mag Calibration: Wave device in a figure eight until done!\n");
    k_sleep(K_MSEC(1000));
    printk("Staring\n");

    sample_count = 1500; 
    for(ii = 0; ii < sample_count; ii++) {
        /* Blink LED to indicate calibration*/
        if (ii % 2) {
            gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
        } else {
            gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
        }

        /* Get magnetometer sample */
        mpu9250_get_mag_sample(dev, mag_temp);

        for (int jj = 0; jj < 3; jj++) {
            if(mag_temp[jj] > mag_max[jj]) mag_max[jj] = mag_temp[jj];
            if(mag_temp[jj] < mag_min[jj]) mag_min[jj] = mag_temp[jj];
        }
        k_sleep(K_MSEC(20));
    }

    // Get hard iron correction
    bias_dest[0] = (mag_max[0] + mag_min[0]) / 2;  // avg. x mag bias
    bias_dest[1] = (mag_max[1] + mag_min[1]) / 2;  // avg. y mag bias
    bias_dest[2] = (mag_max[2] + mag_min[2]) / 2;  // avg. z mag bias 

    // Get soft iron correction estimate
    temp_scale[0] = (mag_max[0] - mag_min[0]) / 2;  // avg. x max chord length
    temp_scale[1] = (mag_max[1] - mag_min[1]) / 2;  // avg. y max chord length
    temp_scale[2] = (mag_max[2] - mag_min[2]) / 2;  // avg. z max chord length

    float avg_rad = (temp_scale[0] + temp_scale[1] + temp_scale[2]) / 3.0f;

    scale_dest[0] = avg_rad / temp_scale[0];
    scale_dest[1] = avg_rad / temp_scale[1];
    scale_dest[2] = avg_rad / temp_scale[2];

    /* Turn LED back on */
    gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    LOG_INF("Calibration done.");

    LOG_INF("  bias: %f, %f, %f", (double)bias_dest[0],
                                       (double)bias_dest[1],
                                       (double)bias_dest[2]);
    LOG_INF("  scale: %f, %f, %f", (double)scale_dest[0],
                                        (double)scale_dest[1],
                                        (double)scale_dest[2]);

    k_sleep(K_MSEC(1000));
}

/* Process and send the sensor sample to the beacon */
static void mpu9250_process_sample(const struct device *dev, float *a, float *g, float *m)
{
    struct sensor_value accel[3];
	struct sensor_value gyro[3];
	struct sensor_value mag[3];

    /* Fetch the sample */
    if (sensor_sample_fetch(dev) < 0) {
        LOG_ERR("Sensor sample update error");
        return;
    }

    /* Read the accelerometer */
    if (sensor_channel_get(dev, SENSOR_CHAN_ACCEL_XYZ, accel) < 0) {
        LOG_ERR("Cannot read accelerometer");
        return;
    }

    /* Read the gyroscope */
    if (sensor_channel_get(dev, SENSOR_CHAN_GYRO_XYZ, gyro) < 0) {
        LOG_ERR("Cannot read gyroscope");
        return;
    }

    /* Read the magnetometer */
    if (sensor_channel_get(dev, SENSOR_CHAN_MAGN_XYZ, mag) < 0) {
        LOG_ERR("Cannot read magnetometer");
        return;
    }

    a[0] = sensor_value_to_float(&accel[0]) * A_RES;
    a[1] = sensor_value_to_float(&accel[1]) * A_RES;
    a[2] = sensor_value_to_float(&accel[2]) * A_RES;

    g[0] = sensor_value_to_float(&gyro[0]) * G_RES;
    g[1] = sensor_value_to_float(&gyro[1]) * G_RES;
    g[2] = sensor_value_to_float(&gyro[2]) * G_RES;

    m[0] = (sensor_value_to_float(&mag[0]) - mag_bias[0]) * M_RES * mag_scale[0];
    m[1] = (sensor_value_to_float(&mag[1]) - mag_bias[1]) * M_RES * mag_scale[1];
    m[2] = (sensor_value_to_float(&mag[2]) - mag_bias[2]) * M_RES * mag_scale[2];

    LOG_INF("\nacc  %f %f %f G", (double)a[0], (double)a[1], (double)a[2]);
    LOG_INF("\ngyro  %f %f %f rad/s", (double)g[0], (double)g[1], (double)g[2]);
    LOG_INF("\nmag  %f %f %f mG", (double)m[0], (double)m[1], (double)m[2]);

}

int init_button(void)
{
	int ret;

	if (!gpio_is_ready_dt(&button)) {
		LOG_ERR("Error: button device %s is not ready\n",
		       button.port->name);
		return 1;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret != 0) {
		LOG_ERR("Error %d: failed to configure %s pin %d\n",
		       ret, button.port->name, button.pin);
		return 1;
	}

	ret = gpio_pin_interrupt_configure_dt(&button,
					      GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		LOG_ERR("Error %d: failed to configure interrupt on %s pin %d\n",
			ret, button.port->name, button.pin);
		return 1;
	}

	gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);
	LOG_INF("Set up button at %s pin %d\n", button.port->name, button.pin);

    return 0;
}

/*
 * Data capture thread for MPU9250
 */
void wsu_mpu9250_thread(void)
{
    const struct device *const mpu9250 = DEVICE_DT_GET_ONE(invensense_mpu9250);

    if (!device_is_ready(mpu9250)) {
        LOG_ERR("device is not ready");
        return;
    }

    /* Init button for calibration */
    if (init_button()) {
        LOG_ERR("Failed to init button.");
        return;
    }

    /* Setup Madgwick filter variables */
    float beta = sqrtf(3.0f / 4.0f) * GYRO_MEAS_ERR; 
    float zeta = sqrtf(3.0f / 4.0f) * GYRO_MEAS_DRIFT;

    /* Time tracking variables */
    uint64_t now = 0;
    uint64_t last_update = 0;
    float time_delta = 0;

    /* Sensor values */
    float accel[3] = {0.0f, 0.0f, 0.0f};
    float gyro[3] = {0.0f, 0.0f, 0.0f};
    float mag[3] = {0.0f, 0.0f, 0.0f};

    /* Quarternion */
    float q[4] = {1.0f, 0.0f, 0.0f, 0.0f};

    /* Sensor fusion variables */
    float pitch = 0.0f;
    float roll = 0.0f;
    float yaw = 0.0f;
    float heading = 0.0f;

    while (true) {

        /* Try and take calibration semaphore */
        if (k_sem_take(&calibration_sem, K_NO_WAIT) == 0) {
            mpu9250_mag_cal(mpu9250, mag_bias, mag_scale);
        }

        /* Get the sample and and update filter */
        now = k_uptime_get();
        time_delta = ((now - last_update) / 1000.0f);
        mpu9250_process_sample(mpu9250, accel, gyro, mag);

        MadgwickQuaternionUpdate(q, time_delta, beta, zeta,
                                 accel[0], accel[1], accel[2],
                                 gyro[0], gyro[1], gyro[2],
                                 mag[0], mag[1], mag[2]);

        last_update = now;
        LOG_INF("\nq: %f %f %f %f", (double)q[0], (double)q[1], (double)q[2], (double)q[3]);

        /* Convert quartenion to pitch, roll and yaw */
        yaw   = atan2f(2.0f * (q[1] * q[2] + q[0] * q[3]), q[0] * q[0] + q[1] * q[1] - q[2] * q[2] - q[3] * q[3]);   
        pitch = -asinf(2.0f * (q[1] * q[3] - q[0] * q[2]));
        roll  = atan2f(2.0f * (q[0] * q[1] + q[2] * q[3]), q[0] * q[0] - q[1] * q[1] - q[2] * q[2] + q[3] * q[3]);
        pitch *= 180.0f / PI;
        yaw   *= 180.0f / PI; 
        yaw   += DECLINATION;
        roll  *= 180.0f / PI;

        /* Convert yaw to 360 degree heading */
        heading = (yaw < 0) ? yaw + 360.0f : yaw;

        // printk("\np=%f r=%f y=%f", (double)pitch, (double)roll, (double)heading);

        /* Send the message to the beacon thread */
        float msg[3];
        msg[0] = pitch;
        msg[1] = roll;
        msg[2] = heading;
        wsu_msg_send(msg, K_NO_WAIT);

        k_sleep(K_MSEC(15));
    }
    return;
}

K_THREAD_DEFINE(wsu_mpu9250, T_MPU9250_STACKSIZE, wsu_mpu9250_thread, NULL,
                NULL, NULL, T_MPU9250_PRIORITY, 0, 0);
