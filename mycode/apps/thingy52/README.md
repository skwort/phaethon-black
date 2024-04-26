# Thingy52

## Implementation Details

### MPU9250
The MPU9250 uses a separate power source and is disabled by default. The startup
`board.c` file had to be modified to initialise this power source. I found guidance
for this modification on [Nordic's DevZone Forum][1].

The following sensor configurations are currently in use:
- gyro sample rate divider 20;
- gyro full-scale +/- 250 rads/s
- gyro lpf cutoff at 10 Hz
- accel full-scale +/- 4g
- accel lpf cutoff at 10.2 Hz

[1]:https://devzone.nordicsemi.com/f/nordic-q-a/64653/thingy52-zephyr-rtos-and-sensor-mpu6060-sample-not-working