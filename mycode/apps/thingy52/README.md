# Thingy52 (Wireless Sensor Unit)

## Implementation Details

### MPU9250
The MPU9250 uses a separate power source and is disabled by default. The startup
`board.c` file had to be modified to initialise this power source. I found guidance
for this modification on [Nordic's DevZone Forum][1].

The following sensor configurations are currently in use:
- gyro sample rate divider 1;
- gyro full-scale +/- 250 rads/s
- gyro lpf cutoff at 10 Hz
- accel full-scale +/- 2g
- accel lpf cutoff at 10.2 Hz

Sensor data is fused into a pitch roll and yaw using a Madgwick filter.
The filter implementation was adpated from from
[kriswiner's MPU9250 driver on Github][2].


[1]:https://devzone.nordicsemi.com/f/nordic-q-a/64653/thingy52-zephyr-rtos-and-sensor-mpu6060-sample-not-working
[2]:https://github.com/kriswiner/MPU9250/tree/master