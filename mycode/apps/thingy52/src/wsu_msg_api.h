#ifndef WSU_MSG_H
#define WSU_MSG_H

#include <sys/_stdint.h>
#include <zephyr/kernel.h>

/* Data types for wsu_msg_t */
#define WSU_PITCH 0x01
#define WSU_ROLL  0x02
#define WSU_YAW   0x03

/* Prototypes */
extern void wsu_msg_send(float *msg, k_timeout_t timeout);
extern void wsu_msg_recv(float *msg, k_timeout_t timeout);

#endif
