import os
import subprocess
import time

RECEIVE_FILENAME = "received.mp3"
SEND_FILENAME = "quacks.mp3"


def measure_bandwidth():
    receiver_process = subprocess.Popen(["../../receiver", "12345", RECEIVE_FILENAME])

    time.sleep(1)

    start = time.time()

    sender_process = subprocess.Popen(
        ["../../sender", "localhost", "12345", SEND_FILENAME, str(os.path.getsize(SEND_FILENAME))]
    )

    receiver_process.wait()
    sender_process.wait()

    end = time.time()

    duration = end - start
    bandwidth_bytes = os.path.getsize(SEND_FILENAME)
    bandwidth_bits = bandwidth_bytes * 8
    bandwidth_mbps = (bandwidth_bits / duration) / (1024 * 1024)

    return bandwidth_mbps, duration


while True:
    bandwidth, duration = measure_bandwidth()
    print("Bandwidth usage over {:.10f} seconds: {:.2f} Mbps".format(duration, bandwidth))
