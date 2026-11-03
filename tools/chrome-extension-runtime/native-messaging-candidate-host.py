#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Owned stdio host used by the real candidate Native Messaging API gate."""
import json
import os
from pathlib import Path
import struct
import sys

MAX_MESSAGE = 1024 * 1024
LOG = Path(__file__).with_name("host-traffic.jsonl")


def record(event, **values):
    with LOG.open("a", encoding="utf-8", newline="\n") as stream:
        stream.write(json.dumps({"event": event, "pid": os.getpid(), **values}, ensure_ascii=False) + "\n")
        stream.flush()
        os.fsync(stream.fileno())


def read_exact(stream, size):
    chunks = bytearray()
    while len(chunks) < size:
        chunk = stream.read(size - len(chunks))
        if not chunk:
            if chunks:
                raise ValueError("Truncated native message frame")
            return None
        chunks.extend(chunk)
    return bytes(chunks)


def main():
    source, destination = sys.stdin.buffer, sys.stdout.buffer
    record("start", argv=sys.argv[1:])
    serial = 0
    clean = False
    failure = None
    try:
        while True:
            header = read_exact(source, 4)
            if header is None:
                break
            size = struct.unpack("=I", header)[0]
            if size > MAX_MESSAGE:
                raise ValueError("Native request exceeds owned fixture bound")
            payload = read_exact(source, size)
            if payload is None:
                raise ValueError("Missing native message payload")
            message = json.loads(payload.decode("utf-8"))
            serial += 1
            record("receive", serial=serial, message=message)
            reply = json.dumps({"pid": os.getpid(), "serial": serial,
                                "argv": sys.argv[1:], "message": message},
                               ensure_ascii=False, separators=(",", ":")).encode("utf-8")
            if len(reply) > MAX_MESSAGE:
                raise ValueError("Native response exceeds owned fixture bound")
            destination.write(struct.pack("=I", len(reply)))
            destination.write(reply)
            destination.flush()
            record("send", serial=serial, bytes=len(reply))
        clean = True
    except Exception as error:
        failure = str(error)
        raise
    finally:
        record("exit", messages=serial, clean=clean, error=failure)


if __name__ == "__main__":
    main()
