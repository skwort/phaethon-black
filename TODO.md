# Phaethon-Black Todo List

This file tracks ongoing tasks, upcoming features, and bugs to fix for the `phaethon-black` project.

## Flynn

- [ ] Test reading in transponder packets with RTL-SDR / dump1090
- [ ] Help Sam create coordinate system orientated around true north
- [ ] Develop ADS-B transponder signal processing functions which parse relevant data to nrf DK
- [ ] Establish communication between Raspberry Pi and NRF52840 DK - using protobuff over UART
- [ ] Design interface / display for M5core2
- [ ] Utilise MQTT pub/sub protocol to receive data from DK and display relevantly
- [ ] Work on a more intricate front end model using PC software

## Sam

- [ ] Establish periphery connection/interfaces with mag and GPS sensors
- [ ] Establish comms link over UART with between rasp pi and nrfDK
- [ ] Parse in sensor data to meaningful data structure of chosen sector
- [ ] Initialise relevant data structures to create 'valid' ADSB packets based on 'section of sky;
- [ ] Receive PB data from pi and publish to MQTT broker
      
