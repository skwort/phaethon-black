# NRF52840DK (Base Station)

## Implementation Details

### BLE State Machine
A state machine is used to transition between different device BT modes:

- IDLE
- CONNECTED
- SCANNING

This enables shell commands to be used to enable and disable connections,
improving the debugging experience.
Three commands are implemented:
- `blescan`
  - This command maintains a filter list of allowable MAC addresses. You can omit
  the MAC to scan for all devices.
  - Running the `-p` command will reset the filter list.

    ```
    Usage:
        blecon -s [MAC ADDRESS]
        blecon -p
    ```
- blecon
  - This command *connects* to a specified MAC address and parses the packets. Note
  that packets are expected to conform to the WSU protocol. No error handling is
  implemented.

    ```
    Usage:
        blecon -s <MAC ADDRESS>  (initiate connection)
        blecon -p                (terminate connection)
    ```
Connections are implemented by selectively filtering for the desired MAC address.
Concurrent connections are supported but unnecessary.

### WSU BLE Link
A BLE beacon topology is used to link the Thingy52 to NRF52840DK base station.
Two implementations are available:
- iBeacon based extended advertising (EXT)
- custom protocol advertising (STD)

The EXT implmentation works well but is has issue with data alignment, as fused
IMU values are sent in separate packets. The STD implementation includes all
IMU data in a single packet, meaning better data coherence. You can switch
between implementations by uncommenting `CONFIG_BT_EXT_ADV` in `prj.conf`. You
also need to do this for the Thingy52.

## Device Link Transfer (DLT)
Given that the NRFDK needs to support BLE, UART, and USB, the DLT API has been
revised to formalise the semantics and increase protocol robustness.

DLT defines *Devices* and *Links*. Devices are where data is generated or
received. Links are where data transfer occurs; they *bridge* devices.

DLT enables Devices and Links to be connected in a one-to-many fashion; a single
device can have many links.

This notion is acheived by defining Link *endpoints*, e.g. UART or I2C. The
implementation of the actual peripheral interface is left up to the user. Users
should define endpoints with an `enum`, and then specify the number of
endpoints when initialising the interface.

The main purpose of this API is to abstract synchronisation and data sharing
away from the user. Users need only define their endpoints in `dlt_endpoints.h`
and implement the peripheral interface, ensuring they call the relevant DLT
link functions.

After defining the endpoints, users need to initialise the interface and register
the device and link threads:

```c
/* Device registration (in main thread) */
k_tid_t device_tid = k_current_get();
dlt_interface_init(NUM_ENDPOINTS);
dlt_device_register(device_tid);

/* Link registration (in peripheral interface thread) */
k_tid_t link_tid = k_current_get();
dlt_link_register(link_tid);
```

Implementation is acheived using Zephyr mailboxes. One mailbox is defined
for each endpoint. The mailbox messages are selected based on thread id, i.e.
`read(ep, ...)` reads the mailbox associated with the endpoint ep, and retrieves
messages sent by the `link thread` associated with ep.

I ran into some issues with the implementation related to asynchronous transfer
and message targeting. I managed to get past them, but it's worth noting that
the DLT API could also be implemented using message queues.

### DLT Implementation 
The NRFDK is basically a data aggregator and forwarder. It needs to communicate
with many devices:
- Thingy52 via BLE
- Web/PC via WebUSB
- RPi via UART
- M5Core2 via BLE?

Using my DLT interface, I can consider each of these devices *Link* endpoints,
and write the peripheral interfaces accordingly.

Then, in my main thread, I can receive and process data, and send it back
through the Links as necessary.

A sample main thread implementation is included below for reference. This
implementation enforces a *every request needs a response* requirement,
but this is not necessary.
```c
int main(void)
{
    k_tid_t device_tid = k_current_get();
    dlt_interface_init(DLT_NUM_ENDPOINTS);
    dlt_device_register(device_tid);

    uint8_t data[] = "test data";
    uint8_t tx_packet[50] = {0};
    uint8_t rx_packet[50] = {0};
    bool resp_pending = false;

    while (true) {
        if (!resp_pending) {
            /* Send a request with data */
            LOG_INF("REQUEST");
            dlt_request(UART_EP, tx_packet, data, 5, true);
            resp_pending = true;

        } else if (resp_pending) {
            /* Get a response */
            uint8_t resp_len = dlt_read(UART_EP, rx_packet, 50, K_FOREVER);
            if (resp_len) { 
                LOG_INF("GOT RESPONSE");
                /* Assuming we are sending a string */
                printk("resp: %s\n", rx_packet);
                resp_pending = false;
            }
            LOG_INF("NO RESPONSE");
        }
        k_sleep(K_MSEC(10));
    }

    return 0;
}
```

### GPS Module Interfacing

CONNECTION - i2c SCL - PO27, i2c SDA PO26

CONF INCLUDES: just i2c, piggybacks off existing log messages

DATA STRUCT: has a valid (0 means not relevant) bool and then latitude and longitude values. Disregard any lat / long data unless the bool is 1 (sattelite lock)

REQUIRED OVERLAY:
```
&i2c0 {
    clock-frequency = <I2C_BITRATE_STANDARD>;
};


&pinctrl {
    i2c0_default: i2c0_default {
		group1 {
			psels = <NRF_PSEL(TWIM_SDA, 0, 26)>,
				<NRF_PSEL(TWIM_SCL, 0, 27)>;
            bias-pull-up;
		};
	};
};
```