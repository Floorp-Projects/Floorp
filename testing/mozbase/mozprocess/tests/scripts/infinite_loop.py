import signal
import sys
import threading
import time

if "deadlock" in sys.argv:
    lock = threading.Lock()

    def trap(sig, frame):
        lock.acquire()

    # get the lock once
    lock.acquire()
    # and take it again on SIGTERM signal: deadlock.
    signal.signal(signal.SIGTERM, trap)

while 1:
    time.sleep(1)
