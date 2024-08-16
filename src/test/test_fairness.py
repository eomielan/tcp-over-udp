import os
import queue
import subprocess
import threading
import time

import pytest

CONVERGENCE_THRESHOLD = 0.1  # 10% threshold
MAX_RTT_COUNT = 100
PORT1 = 12345
PORT2 = 12346
HOSTNAME = "localhost"


def start(result_queue, port, send_filename, receive_filename):
    start = time.time()
    receiver = subprocess.Popen(["../../receiver", str(port), receive_filename])
    sender = subprocess.Popen(
        [
            "../../sender",
            HOSTNAME,
            str(port),
            send_filename,
            str(os.path.getsize(send_filename)),
        ]
    )
    receiver.wait()
    sender.wait()
    end = time.time()
    result_queue.put(end - start)


@pytest.mark.parametrize(
    "send_filename, receive_filename1, receive_filename2",
    [
        ("sample.txt", "received1.txt", "received2.txt"),
        ("hotpot.jpg", "received1.jpg", "received2.jpg"),
        ("quacks.mp3", "received1.mp3", "received2.mp3"),
    ],
)
def test_fairness(send_filename, receive_filename1, receive_filename2):
    for _ in range(5):
        result_queue = queue.Queue()

        thread1 = threading.Thread(
            target=start,
            args=(
                result_queue,
                PORT1,
                send_filename,
                receive_filename1,
            ),
        )
        thread2 = threading.Thread(
            target=start,
            args=(
                result_queue,
                PORT2,
                send_filename,
                receive_filename2,
            ),
        )

        thread1.start()
        thread2.start()

        thread1.join()
        thread2.join()

        time1 = result_queue.get()
        time2 = result_queue.get()

        transfer_bytes = os.path.getsize(send_filename)

        throughput1 = transfer_bytes / time1
        throughput2 = transfer_bytes / time2
        throughputs = (throughput1, throughput2)

        fairness_ratio = max(throughputs) / min(throughputs)
        print(fairness_ratio)

        assert fairness_ratio <= 1 + CONVERGENCE_THRESHOLD
        assert fairness_ratio >= 1 - CONVERGENCE_THRESHOLD


if __name__ == "__main__":
    pytest.main(["-v"])
