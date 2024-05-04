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
