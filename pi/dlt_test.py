import dlt
import logging
import time

logging.basicConfig(level=logging.INFO)

UART_PORT = "/dev/serial/by-id/usb-SEGGER_J-Link_001050234086-if00"

# Connect to interface
dlt_if = dlt.DLTInterface(backend="serial", port=UART_PORT)


try:
    while True:
        data = dlt_if.read()
        if data is not None:
            msg_type, read_data = data 
            print(read_data)
            resp = "resp".encode(encoding="ascii")
            print("responding")
            dlt_if.respond(resp)

        time.sleep(0.5)
except KeyboardInterrupt:
    dlt_if.close()
