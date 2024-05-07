#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

#include "dlt_api.h"
#include "dlt_endpoints.h"

/* Thread parameters */
#define DLT_COMMS_STACKSIZE 1024
#define DLT_COMMS_PRIORITY  6

/* DLT COMMS State Machine States */
#define DLT_COMMS_IDLE             0
#define DLT_COMMS_RESPONSE_PENDING 1

/* Buffer sizes */
#define DLT_UART_RX_BUF_LEN   100
#define DLT_RECV_BUF_LEN      100
#define DLT_SEND_BUF_LEN      100 

#define DLT_RX_TIMEOUT (25 * USEC_PER_MSEC)

LOG_MODULE_REGISTER(dlt_uart_link, LOG_LEVEL_INF);

// UART callback prototype
static void uart_cb(const struct device *dev, struct uart_event *evt,
                    void *user_data);

/* UART device reference*/
#ifdef CONFIG_SHELL_BACKEND_RTT
static const struct device *dlt_uart =
    DEVICE_DT_GET(DT_CHOSEN(zephyr_shell_uart));
#else
static const struct device *dlt_uart = DEVICE_DT_GET(DT_ALIAS(dlt_uart));
#endif

/* DLT COMMS state variable*/
static uint8_t dlt_comms_state = DLT_COMMS_IDLE;

/* UART DMA Semaphore for data reception */
K_SEM_DEFINE(dlt_rx_sem, 0, 1);

/* Intialise the DLT UART Link thread */
extern void dlt_uart_init()
{
    int ret = uart_callback_set(dlt_uart, uart_cb, NULL);
    if (ret) {
        LOG_ERR("DLT COMMS init failed.");
        return;
    }
}

/*
 * Thread function for DLT Communication.
 *
 * @brief This is a state machine with and IDLE and PENDING state. The IDLE
 *        state waits for DLT packets to appear in the DLT mailbox. Upon
 *        receiving mail, the thread sends the DLT packet via UART and
 *        transitions into the PENDING state. In the pending state, the thread
 *        waits for a response via UART.
 */
void dlt_uart_thread(void)
{
    k_tid_t link_tid = k_current_get();
    dlt_uart_init();
    dlt_link_register(PI_UART, link_tid);

    uint8_t dlt_recv_buf[DLT_RECV_BUF_LEN] = {0};
    uint8_t uart_recv_buffer[DLT_UART_RX_BUF_LEN] = {0};
    k_sleep(K_MSEC(100));

    while (1) {
        uint8_t msg_len;
        switch (dlt_comms_state) {
        case DLT_COMMS_IDLE:
            /* Poll DLT Interface for packets to transmit */
            msg_len = dlt_poll(PI_UART, dlt_recv_buf, 50, K_FOREVER);
            if (!msg_len) {
                break;
            }

            LOG_INF("DLT mail received.");

            /* Transmit the DLT packet via UART */
            LOG_INF("Transmitting DLT packet.");
            for (int i = 0; i < msg_len; i++) {
                LOG_INF("  packet[%i]: %" PRIx8, i, dlt_recv_buf[i]);
            }
            int ret =
                uart_tx(dlt_uart, dlt_recv_buf, msg_len, SYS_FOREVER_US);
            if (ret) {
                LOG_ERR("DLT UART transmission failed.");
                break;
            }

            /* Start UART RX */
            ret = uart_rx_enable(dlt_uart, uart_recv_buffer,
                                 DLT_UART_RX_BUF_LEN, DLT_RX_TIMEOUT);
            if (ret) {
                LOG_ERR("DLT RX enable failed.");
                break;
            }

            dlt_comms_state = DLT_COMMS_RESPONSE_PENDING;
            break;

        case DLT_COMMS_RESPONSE_PENDING:
            /* Wait for UART DMA RX completion signal */
            if (k_sem_take(&dlt_rx_sem, K_MSEC(100)) != 0) {
                break;
            }

            LOG_INF("Response received, submitting.");
            dlt_submit(PI_UART, uart_recv_buffer, uart_recv_buffer[2] + 3, false);

            /* Back to IDLE state */
            dlt_comms_state = DLT_COMMS_IDLE;
            break;

        default:
            dlt_comms_state = DLT_COMMS_IDLE;
            break;
        }
        k_sleep(K_MSEC(5));
    }
}

/* Register the thread */
K_THREAD_DEFINE(dlt_comms, DLT_COMMS_STACKSIZE, dlt_uart_thread, NULL, NULL,
                NULL, DLT_COMMS_PRIORITY, 0, 0);

/*
 * UART Async API callback function
 *
 * @brief The callback function gives the dlt_rx semaphore when RX is complete.
 */
static void uart_cb(const struct device *dev, struct uart_event *evt,
                    void *user_data)
{
    switch (evt->type) {

    case UART_TX_DONE:

        break;

    case UART_RX_RDY:
        /* Disable UART after the RX is ready */
        uart_rx_disable(dlt_uart);
        break;

    case UART_RX_DISABLED:
        break;

    case UART_RX_STOPPED:
        LOG_ERR("UART DMA ERROR.");
        break;

    case UART_RX_BUF_REQUEST:
        break;

    case UART_RX_BUF_RELEASED:
        LOG_INF("UART DMA reception complete.");
        LOG_INF("Signalling thread.");
        k_sem_give(&dlt_rx_sem);
        break;

    default:
        break;
    }
}