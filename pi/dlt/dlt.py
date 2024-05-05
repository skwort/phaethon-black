import serial
import logging
import time
import queue
import threading

logger = logging.getLogger(__name__)

# DLT Protocol Codes
DLT_PREAMBLE = 0x77
DLT_REQUEST_CODE = 0x01
DLT_RESPONSE_CODE = 0x02


# Base class for DLT Backend
class DLTBackend(threading.Thread):
    """
    Represents a backend for the DLT interface, responsible for managing
    the communication with the device.

    Parameters:
        interface_read_queue (Queue): The queue for storing received data.

    Methods:
        submit_write(item) -> None:
            Submits an item to be written to the device.

        run() -> None:
            The main loop of the backend thread.

        stop() -> None:
            Stops the backend thread.
    """
    def __init__(self, interface_read_queue: queue.Queue):
        super().__init__()
        self._if_read_queue = interface_read_queue
        self._write_queue = queue.Queue()
        self._stop_event = threading.Event()

    def _write(self, s: bytes) -> None:
        raise NotImplementedError("write() method is not implemented")

    def _disconnect(self):
        raise NotImplementedError("disconnect() method is not implemented")

    def _connect(self):
        raise NotImplementedError("connect() method is not implemented")

    def _read(self, n_bytes=1) -> bytes:
        raise NotImplementedError("_read() method is not implemented")

    def _is_connected(self):
        raise NotImplementedError("is_connected() method is not implemented")

    def _read_packet(self) -> tuple:
        dlt_length = None
        count = 0
        packet = bytes()
        data = bytes()
        while True:
            if dlt_length is not None and (count - 3) == dlt_length:
                logger.info("DLT packet reception complete.")
                logger.info("Aborting listen.")
                break

            byte = self._read()
            if byte is None:
                return None, None

            if count == 0:
                if int.from_bytes(byte) != DLT_PREAMBLE:
                    return None, None
                logger.info("DLT packet received.")
            elif count == 1:
                msg_code = int.from_bytes(byte)
            elif count == 2:
                dlt_length = int.from_bytes(byte)
            else:
                data += byte

            packet += byte
            count += 1

        self._log_dlt_packet(packet)
        return msg_code, data

    def _log_dlt_packet(self, packet, log_data=False):
        logger.info("==================================")
        for count, byte in enumerate(packet):

            if count == 0:
                logger.info(f"PREAMBLE: 0x{byte:02X}")

            elif count == 1:
                msg_code = byte
                msg_str = "REQUEST"
                if msg_code == 0x02:
                    msg_str = "RESPONSE"
                logger.info(f"MSG_TYPE: 0x{msg_code:02X} - {msg_str}")

            elif count == 2:
                hci_length = byte
                logger.info(f"  LENGTH: {hci_length:>4}")

            elif count > 2 and log_data:
                logger.info(f"  DATA[{count}]: 0x{byte:02X}")
            else:
                break
        logger.info("==================================")

    def submit_write(self, item):
        self._write_queue.put(item)

    def run(self):
        while True:
            # Terminate if stop condition is met
            if self._stop_event.is_set():
                break

            # Read packet
            msg_code, data = self._read_packet()
            if msg_code is not None and data is not None and len(data) > 0:
                # Forward data to DLT interface
                self._if_read_queue.put((msg_code, data))

            # Check if we need to write data
            if not self._write_queue.empty():
                write_item = self._write_queue.get()
                self._write(write_item)

            time.sleep(0.001)

        self._disconnect()

    def stop(self):
        self._stop_event.set()


class SerialBackend(DLTBackend):
    """
    Represents a serial backend within the DLT interface.

    Parameters:
        interface_read_queue (Queue): The queue for storing received data.
        port (str): The port to use for the serial connection.
    """

    def __init__(self, interface_read_queue, port: str) -> None:
        super().__init__(interface_read_queue)
        self.port = port
        self._conn = None
        self._connect()

        # Start
        self.start()

    def _connect(self, baudrate=115200) -> None:
        """ Connect to the device using the specified port """
        self._conn = serial.Serial(self.port, baudrate, timeout=0.25)
        self._conn.reset_input_buffer()

    def _disconnect(self):
        """ Disconnect from the device """
        self._conn.close()

    def _write(self, s: str) -> None:
        """ Write a string to the device. """
        if not self._conn:
            return

        self._conn.write(s)
        logger.info(f"Wrote {len(s)} bytes.")

    def _read(self, n_bytes=1) -> str | None:
        """ Read a single byte from the device. """
        if not self._conn:
            return None
        try:
            s = self._conn.read(size=n_bytes)
            return s
        except serial.SerialTimeoutException:
            return None

    def _is_connected(self) -> bool:
        if not self._conn:
            return False

        return self._conn.is_open


class DLTInterface:
    """
    The DLT Interface provides a generic interface for data transport,
    abstracting the details of the underlying communication backend.

    Parameters:
        backend (str): The type of backend to use. Only 'serial' is supported.
        port (str): The port to use with the serial backend.

    Methods:
        request(data: bytes) -> None:
            Sends a request message through the DLT interface.

        respond(data: bytes) -> None:
            Sends a response message through the DLT interface.

        read() -> tuple | None:
            Reads a message from the DLT interface.

        close() -> None:
            Closes the DLT interface and stops the backend.
    """
    def __init__(self, backend="serial", port="/dev/ttyACM0") -> None:
        self._read_queue = queue.Queue()

        # Create the backend
        try:
            if backend == "serial":
                self._backend = SerialBackend(self._read_queue, port)
            else:
                raise NotImplementedError(f"Backend {backend} not supported.")
        except ConnectionError:
            logger.error("Backend failed to connect")

    def request(self, data) -> None:
        self._write(data, DLT_REQUEST_CODE)

    def respond(self, data) -> None:
        self._write(data, DLT_RESPONSE_CODE)

    def read(self) -> tuple | None:
        if self._read_queue.empty():
            return None
        return self._read_queue.get_nowait()

    def close(self) -> None:
        # Stop the backend thread
        self._backend.stop()
        self._backend.join()

    def _generate_dlt_packet(self, data, msg_type) -> bytearray:
        # Create a byte array from data
        byte_array = bytearray(data)

        # Insert the DLT protocol bytes
        byte_array.insert(0, DLT_PREAMBLE)
        byte_array.insert(1, msg_type)
        byte_array.insert(2, len(data))

        return byte_array

    def _write(self, data, msg_type) -> None:
        # Generate the DLT packet and submit to backend
        packet = self._generate_dlt_packet(data, msg_type)
        self._backend.submit_write(packet)
