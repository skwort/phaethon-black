#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(wsu_mpu9250_module);

/* Thread parameters */
#define T_MPU9250_STACKSIZE 512
#define T_MPU9250_PRIORITY  7

/* Process and send the sensor sample to the beacon */
static void mpu9250_process_sample(const struct device *dev)
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

    /* Print the values */
    printk("\naccel %f %f %f m/s/s\n"
           "gyro  %f %f %f rad/s\n"
           "mag  %f %f %f rad/s\n",
           sensor_value_to_double(&accel[0]),
           sensor_value_to_double(&accel[1]),
           sensor_value_to_double(&accel[2]),
           sensor_value_to_double(&gyro[0]),
           sensor_value_to_double(&gyro[1]),
           sensor_value_to_double(&gyro[2]),
           sensor_value_to_double(&mag[0]),
           sensor_value_to_double(&mag[1]),
           sensor_value_to_double(&mag[2]));

}

/*
 * Data capture thread for MPu9250
 */
void wsu_mpu9250_thread(void)
{
    const struct device *const mpu9250 = DEVICE_DT_GET_ONE(invensense_mpu9250);

    if (!device_is_ready(mpu9250)) {
        LOG_ERR("device is not ready");
        return;
    }

    while (true) {
        mpu9250_process_sample(mpu9250);
        k_sleep(K_MSEC(20));
    }
    return;
}

K_THREAD_DEFINE(wsu_mpu9250, T_MPU9250_STACKSIZE, wsu_mpu9250_thread, NULL,
                NULL, NULL, T_MPU9250_PRIORITY, 0, 0);