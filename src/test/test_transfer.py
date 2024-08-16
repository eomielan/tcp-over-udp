import os
import subprocess

import pytest


@pytest.mark.parametrize(
    "send_filename, receive_filename",
    [
        ("sample.txt", "received.txt"),
        ("hotpot.jpg", "received.jpg"),
        ("quacks.mp3", "received.mp3"),
    ],
)
def test_file_transfer(send_filename, receive_filename):
    # Clear received file before each test
    with open(receive_filename, "wb"):
        pass

    with open(receive_filename, "rb") as received_file:
        assert received_file.read() == b""

    with open(send_filename, "rb") as send_file:
        send_data = send_file.read()

    receiver_process = subprocess.Popen(["../../receiver", "12345", receive_filename])

    sender_process = subprocess.Popen(
        ["../../sender", "localhost", "12345", send_filename, str(os.path.getsize(send_filename))]
    )

    sender_process.wait()
    receiver_process.wait()

    with open(receive_filename, "rb") as received_file:
        received_data = received_file.read()

    assert len(send_data) == len(received_data)
    assert send_data == received_data


if __name__ == "__main__":
    pytest.main(["-v"])
