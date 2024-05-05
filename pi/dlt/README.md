# Device Link Transfer
This Python module is used to create a data agnostic link between devices.
It's based on a HCI type protocol I implemented in CSSE4011.

The packet format is as follows:
```
PACKET[0] = preamble, 0x77 
PACKET[1] = message type, REQUEST 0x01 or RESPONSE 0x02 
PACKET[2] = length of data section
PACKET[3:] = data section 
```

The DLT interface exposes three key methods:
- `request`, for requesting things
- `respond`, for responding to requests
- `read`, for reading avaialble packets

The DLT interface has a generic backend which wraps the IO logic in a 
`threading.Thread.run` function. Custom backends can easily be implemented
to use any physical layer, e.g. WebSockets, HTTP, BLE, etc., by extending
the DLTBackend class.

Currently, only a PySerial based serial backend is implemented. There are some
quirks in the implementation related to port selection and baudrate; these may
need to be physically set.

## Implementation
`DLTInterface` has a read queue and `DLTBackend` thread has a write 
queue. `DLTInterface` submits packets to `DLTBackend`'s write queue
for transmission. The `DLTBackend` thread loop continuously reads the
backend interface for packets. Packets are then submitted to
`DLTInterface`'s read queue.

## Tests
Tests are implemented for the PySerial backend using a virtual serial port.

```
python -m pytest tests/test_dlt.py
```

