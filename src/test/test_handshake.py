import os
import subprocess
import time

import pytest


@pytest.fixture(autouse=True)
def clear_received_file():
    """Clear received.txt before each test"""
    with open("received.txt", "w"):
        pass


@pytest.mark.parametrize("send_filename", ["sample.txt"])
def test_sender_starts_first(send_filename):
    with open("received.txt", "r") as received_file:
        assert received_file.read() == ""

    with open(send_filename, "r") as send_file:
        send_data = send_file.read()

    sender_process = subprocess.Popen(
        ["../../sender", "localhost", "12345", send_filename, str(os.path.getsize(send_filename))]
    )

    time.sleep(2)

    receiver_process = subprocess.Popen(["../../receiver", "12345", "received.txt", "0"])

    sender_process.wait()
    receiver_process.wait()

    with open("received.txt", "r") as received_file:
        received_data = received_file.read()

    assert len(send_data) == len(received_data)
    assert send_data == received_data


@pytest.mark.parametrize("send_filename", ["sample.txt"])
def test_receiver_starts_first(send_filename):
    with open("received.txt", "r") as received_file:
        assert received_file.read() == ""

    with open(send_filename, "r") as send_file:
        send_data = send_file.read()

    receiver_process = subprocess.Popen(["../../receiver", "12345", "received.txt", "0"])

    time.sleep(2)

    sender_process = subprocess.Popen(
        ["../../sender", "localhost", "12345", send_filename, str(os.path.getsize(send_filename))]
    )

    sender_process.wait()
    receiver_process.wait()

    with open("received.txt", "r") as received_file:
        received_data = received_file.read()

    assert len(send_data) == len(received_data)
    assert send_data == received_data


if __name__ == "__main__":
    pytest.main(["-v"])
