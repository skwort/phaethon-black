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

## Thingy52 and NRF52840DK Link
A BLE beacon topology is used to link the Thingy52 to NRF52840DK base station.
Two implementations are available:
- iBeacon based extended advertising (EXT)
- custom protocol advertising (STD)

The EXT implmentation works well but is has issue with data alignment, as fused
IMU values are sent in separate packets. The STD implementation includes all
IMU data in a single packet, meaning better data coherence. You can switch
between implementations by uncommenting `CONFIG_BT_EXT_ADV` in `prj.conf`. You
also need to do this for the base station.

[1]:https://devzone.nordicsemi.com/f/nordic-q-a/64653/thingy52-zephyr-rtos-and-sensor-mpu6060-sample-not-working
[2]:https://github.com/kriswiner/MPU9250/tree/master