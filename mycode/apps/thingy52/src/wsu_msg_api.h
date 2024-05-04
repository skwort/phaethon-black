#ifndef WSU_MSG_H
#define WSU_MSG_H

#include <sys/_stdint.h>
#include <zephyr/kernel.h>

/* Prototypes */
extern void wsu_msg_send(float *msg, k_timeout_t timeout);
extern void wsu_msg_recv(float *msg, k_timeout_t timeout);

#endif
