# Phaethon-Black Todo List

This file tracks ongoing tasks, upcoming features, and bugs to fix for the `phaethon-black` project.

## Flynn

- [ ] Test reading in transponder packets with RTL-SDR / dump1090
- [ ] Develop ADS-B transponder signal processing functions which parse relevant data to nrf DK
- [ ] Establish communication between Raspberry Pi and NRF52840 DK - using protobuf over UART
- [ ] Design interface / display for M5core2
- [ ] Utilise MQTT pub/sub protocol to receive data from DK and display relevantly
- [ ] Work on a more intricate front end model using PC software

## Sam

- [x] Establish peripheral interface with interface with IMU
- [x] Apply sensor fusion to IMU data
- [ ] Establish BLE link between Thingy52 and NRF52840DK
- [ ] Establish peripheral interface with GPS sensor
- [ ] Establish UART link between RPi and NRF52840DK
- [ ] Filter ADSB packets using calculated sky patch
- [ ] Establish BLE link with M5Core2 and send filtered data
      
