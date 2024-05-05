import os
import pty
import dlt
import pytest
import time


@pytest.fixture
def dlt_serial():
    # Create a virtual serial port
    master, slave = pty.openpty()

    # Create the DLTInterface with serial backend
    dlt_if = dlt.DLTInterface(backend="serial", port=os.ttyname(slave))
    yield dlt_if, master

    # Teardown the interface
    dlt_if.close()


def validate_dlt_packet(packet, input_data, msg_type):
    # Check preamble byte
    assert packet[0] == dlt.DLT_PREAMBLE

    # Check request/response byte
    assert packet[1] == msg_type

    # Check length byte
    assert packet[2] == len(input_data)

    # Check data segment
    assert packet[3:].decode() == input_data


def test_dlt_if_ser_request(dlt_serial):
    # Setup dummy data
    data = "test"
    data_bytes = data.encode()

    dlt_if, master = dlt_serial
    dlt_if.request(data_bytes)

    # Read from master
    packet = os.read(master, len(data) + 3)
    validate_dlt_packet(packet, data, dlt.DLT_REQUEST_CODE)


def test_dlt_if_ser_respond(dlt_serial):
    # Setup dummy data
    data = "test"
    data_bytes = data.encode()

    dlt_if, master = dlt_serial
    dlt_if.respond(data_bytes)

    # Read from master
    packet = os.read(master, len(data) + 3)
    validate_dlt_packet(packet, data, dlt.DLT_RESPONSE_CODE)


def test_dlt_if_ser_read_pass(dlt_serial):
    dlt_if, master = dlt_serial

    # Setup dummy data
    data = "test"
    packet = dlt_if._generate_dlt_packet(data.encode(), dlt.DLT_REQUEST_CODE)

    # Imitate an external device writing to DLT interface
    os.write(master, packet)
    time.sleep(0.01)

    # Read and validate the packet
    msg_type, read_data = dlt_if.read()
    assert msg_type == dlt.DLT_REQUEST_CODE
    assert read_data.decode() == data


def test_dlt_if_ser_read_no_data_available(dlt_serial):
    dlt_if, master = dlt_serial

    # Check that None is returned when no data is available
    assert dlt_if.read() is None
