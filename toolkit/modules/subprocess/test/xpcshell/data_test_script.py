#!/usr/bin/env python3

import os
import signal
import struct
import sys


def output(line, stream=sys.stdout, print_only=False):
    if isinstance(line, str):
        line = line.encode("utf-8", "surrogateescape")
    if not print_only:
        stream.buffer.write(struct.pack("@I", len(line)))
    stream.buffer.write(line)
    stream.flush()


def echo_loop():
    while True:
        line = sys.stdin.buffer.readline()
        if not line:
            break

        output(line)


if sys.platform == "win32":
    import msvcrt

    msvcrt.setmode(sys.stderr.fileno(), os.O_BINARY)


cmd = sys.argv[1]
if cmd == "echo":
    echo_loop()
elif cmd == "exit":
    sys.exit(int(sys.argv[2]))
elif cmd == "env":
    for var in sys.argv[2:]:
        output(os.environ.get(var, "!"))
elif cmd == "pwd":
    output(os.path.abspath(os.curdir))
elif cmd == "print_args":
    for arg in sys.argv[2:]:
        output(arg)
elif cmd == "ignore_sigterm":
    signal.signal(signal.SIGTERM, signal.SIG_IGN)

    output("Ready")
    while True:
        try:
            signal.pause()
        except AttributeError:
            import time

            time.sleep(3600)
elif cmd == "print":
    output(sys.argv[2], stream=sys.stdout, print_only=True)
    output(sys.argv[3], stream=sys.stderr, print_only=True)
