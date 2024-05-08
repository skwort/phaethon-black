#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

#include "dlt_api.h"
#include "dlt_endpoints.h"

/* Thread parameters */
#define DLT_COMMS_STACKSIZE 1024
#define DLT_COMMS_PRIORITY  6

/* Timeout for UART DMA */
#define DLT_UART_RX_TIMEOUT (10 * USEC_PER_MSEC)

LOG_MODULE_REGISTER(dlt_uart_link, LOG_LEVEL_INF);

/* UART DMA Semaphore for data reception */
K_SEM_DEFINE(dlt_rx_sem, 0, 1);

/* UART device reference */
#ifdef CONFIG_SHELL_BACKEND_RTT
static const struct device *dlt_uart =
    DEVICE_DT_GET(DT_CHOSEN(zephyr_shell_uart));
#else
static const struct device *dlt_uart = DEVICE_DT_GET(DT_ALIAS(dlt_uart));
#endif

/* UART callback prototype */
static void uart_cb(const struct device *dev, struct uart_event *evt,
                    void *user_data);

/* Intialise the DLT UART Link thread */
extern void dlt_uart_init()
{
    int ret = uart_callback_set(dlt_uart, uart_cb, NULL);
    if (ret) {
        LOG_ERR("DLT COMMS init failed.");
        return;
    }
}

/**
 * Thread function for DLT Communication.
 * 
 * @brief Periodically polls the DLT interface for packets to transmit and 
 *        forwards any received packets.
 */
void dlt_uart_thread(void)
{
    /* Initialise the UART peripheral and register Link with DLT driver */
    k_tid_t link_tid = k_current_get();
    dlt_uart_init();
    dlt_link_register(PI_UART, link_tid);

    /* Buffers */
    uint8_t dlt_recv_buf[DLT_MAX_PACKET_LEN] = {0};
    uint8_t uart_recv_buffer[DLT_MAX_PACKET_LEN] = {0};

    /* State variables */
    bool rx_on = false;
    int ret = 0;

    /* Sleep to let the main thread setup DLT */
    k_sleep(K_MSEC(100));

    while (1) {
        /* Poll DLT Interface for packets to transmit */
        uint8_t msg_len = dlt_poll(PI_UART, dlt_recv_buf, 50, K_MSEC(5));
        if (msg_len) {
            LOG_INF("DLT mail received.");

            /* Transmit the DLT packet via UART */
            LOG_INF("Transmitting DLT packet.");
            for (int i = 0; i < msg_len; i++) {
                LOG_INF("  packet[%i]: %" PRIx8, i, dlt_recv_buf[i]);
            }
            ret = uart_tx(dlt_uart, dlt_recv_buf, msg_len, SYS_FOREVER_US);
            if (ret) {
                LOG_ERR("DLT UART transmission failed.");
                break;
            }
        }

        /* Receive DLT messsages */ 
        if (!rx_on) {
            /* Start UART RX */
            ret = uart_rx_enable(dlt_uart, uart_recv_buffer,
                                 DLT_MAX_PACKET_LEN, DLT_UART_RX_TIMEOUT);
            if (ret) {
                LOG_ERR("DLT UART RX enable failed.");
                break;
            }
            rx_on = true;

        } else if (rx_on && (k_sem_take(&dlt_rx_sem, K_NO_WAIT) == 0)) {
            /* Submit the whole packet to DLT interface */
            LOG_INF("DLT UART packet received, %d bytes. Submitting.",
                    uart_recv_buffer[2] + DLT_PROTOCOL_BYTES);
            dlt_submit(PI_UART, uart_recv_buffer,
                       uart_recv_buffer[2] + DLT_PROTOCOL_BYTES, true);
            rx_on = false;
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
        LOG_ERR("DLT UART DMA ERROR.");
        break;

    case UART_RX_BUF_REQUEST:
        break;

    case UART_RX_BUF_RELEASED:
        LOG_INF("DLT UART DMA reception complete.");
        LOG_INF("Signalling thread.");
        k_sem_give(&dlt_rx_sem);
        break;

    default:
        break;
    }
}