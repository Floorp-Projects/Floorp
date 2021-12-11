import signal
import time

signal.pthread_sigmask(signal.SIG_SETMASK, {signal.SIGTERM})


def main():
    while True:
        time.sleep(1)


if __name__ == "__main__":
    main()
