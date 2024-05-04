#include "wsu_msg_api.h"
#include <zephyr/kernel.h>

/* Define the message queue */
K_MSGQ_DEFINE(wsu_msgq, sizeof(float) * 3, 10, 1);

/* Send a message via the messsage queue */
extern void wsu_msg_send(float *msg, k_timeout_t timeout)
{
    k_msgq_put(&wsu_msgq, msg, timeout);
}

/* Receive a message from the message queue */
extern void wsu_msg_recv(float *msg, k_timeout_t timeout)
{
    k_msgq_get(&wsu_msgq, msg, timeout);
}