# Raspberry Pi
The Raspberry Pi is used to receive ADS-B radio packets with an RTL-SDR.
Received packets are forwarded over a serial connection to a specific device.

## Installation
To install the Python dependencies, first make a virtual env.
```
python -m venv .venv
```
Then active the virtual env.
```
source pi/.venv/bin/activate
```
Now install the dependencies:
```
pip install -r requirements.txt
```

Next, build `dump1090`:
```
cd dump1090
make
```
You may need to install the relevant RTL-SDR drivers.

## Configuration
Adjust the following variables in `adsb_reader.py` as necessary:

- `ADSB_DATA_SERVER_ADDRESS`, the `dump1090` server URL
- `DUMP1090_PATH`, the path to the `dump1090` executable
- `UART_PORT`, the serial port which data should sent to
