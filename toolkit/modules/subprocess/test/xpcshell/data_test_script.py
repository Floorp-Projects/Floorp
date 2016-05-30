#!/usr/bin/env python2
from __future__ import print_function

import os
import signal
import struct
import sys


def output(line):
    sys.stdout.write(struct.pack('@I', len(line)))
    sys.stdout.write(line)
    sys.stdout.flush()


def echo_loop():
    while True:
        line = sys.stdin.readline()
        if not line:
            break

        output(line)


if sys.platform == "win32":
    import msvcrt
    msvcrt.setmode(sys.stderr.fileno(), os.O_BINARY)


cmd = sys.argv[1]
if cmd == 'echo':
    echo_loop()
elif cmd == 'exit':
    sys.exit(int(sys.argv[2]))
elif cmd == 'env':
    for var in sys.argv[2:]:
        output(os.environ.get(var, ''))
elif cmd == 'pwd':
    output(os.path.abspath(os.curdir))
elif cmd == 'print_args':
    for arg in sys.argv[2:]:
        output(arg)
elif cmd == 'ignore_sigterm':
    signal.signal(signal.SIGTERM, signal.SIG_IGN)

    output('Ready')
    while True:
        try:
            signal.pause()
        except AttributeError:
            import time
            time.sleep(3600)
elif cmd == 'print':
    sys.stdout.write(sys.argv[2])
    sys.stderr.write(sys.argv[3])
