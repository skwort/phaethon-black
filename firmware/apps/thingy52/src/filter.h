/**
 * @file filter.h
 * @brief Header file for Madgwick sensor fusion filter
 *
 * This file contains the function declarations for a Madgwick sensor fusion
 * filter. The filter is adapted from kriswiner's MPU9250 driver on Github.
 * The filter filter fuses accelerometer, gyroscope, and magnetometer data to
 * estimate the device's orientation in quarternion format.
 *
 * @author Sam Kwort
 * @date 28/04/2024
 * 
 * @see https://github.com/kriswiner/MPU9250/blob/master/quaternionFilters.ino
 */

#ifndef FILTER_H
#define FILTER_H

/**
 * @brief Update quaternion using Madgwick sensor fusion filter
 *
 * Updates the quaternion using Madgwick's sensor fusion algorithm.
 *
 * @param q Quarternion array
 * @param delta Time since last update
 * @param beta Filter beta parameter
 * @param zeta Filter zeta parameter
 * @param ax Accelerometer x-axis reading
 * @param ax Accelerometer x-axis reading
 * @param ay Accelerometer y-axis reading
 * @param az Accelerometer z-axis reading
 * @param gx Gyroscope x-axis reading
 * @param gy Gyroscope y-axis reading
 * @param gz Gyroscope z-axis reading
 * @param mx Magnetometer x-axis reading
 * @param my Magnetometer y-axis reading
 * @param mz Magnetometer z-axis reading
 */
void MadgwickQuaternionUpdate(float *q, float deltat, float beta, float zeta,
                              float ax, float ay, float az,
                              float gx, float gy, float gz,
                              float mx, float my, float mz);

#endif