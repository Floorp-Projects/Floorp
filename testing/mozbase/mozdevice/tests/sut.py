#!/usr/bin/env python

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import socket
import mozdevice
from threading import Thread
import unittest
import sys
import time

class BasicTest(unittest.TestCase):

    def _serve_thread(self):
        conn = None
        while self.commands:
            if not conn:
                conn, addr = self._sock.accept()
                conn.send("$>\x00")
            (command, response) = self.commands.pop(0)
            data = conn.recv(1024).strip()
            self.assertEqual(data, command)
            # send response and prompt separately to test for bug 789496
            # FIXME: Improve the mock agent, since overloading the meaning
            # of 'response' is getting confusing.
            if response is None:
                conn.shutdown(socket.SHUT_RDWR)
                conn.close()
                conn = None
            elif type(response) is int:
                time.sleep(response)
            else:
                conn.send("%s\n" % response)
                conn.send("$>\x00")

    def _serve(self, commands):
        self.commands = commands
        thread = Thread(target=self._serve_thread)
        thread.start()
        return thread

    def test_init(self):
        """Tests DeviceManager initialization."""
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.bind(("127.0.0.1", 0))
        self._sock.listen(1)

        thread = self._serve([("testroot", "/mnt/sdcard"),
                              ("cd /mnt/sdcard/tests", ""),
                              ("cwd", "/mnt/sdcard/tests"),
                              ("ver", "SUTAgentAndroid Version XX")])

        port = self._sock.getsockname()[1]
        mozdevice.DroidSUT.debug = 4
        d = mozdevice.DroidSUT("127.0.0.1", port=port)
        thread.join()

    def test_reconnect(self):
        """Tests DeviceManager initialization."""
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.bind(("127.0.0.1", 0))
        self._sock.listen(1)

        thread = self._serve([("testroot", "/mnt/sdcard"),
                              ("cd /mnt/sdcard/tests", ""),
                              ("cwd", None),
                              ("cd /mnt/sdcard/tests", ""),
                              ("cwd", "/mnt/sdcard/tests"),
                              ("ver", "SUTAgentAndroid Version XX")])

        port = self._sock.getsockname()[1]
        mozdevice.DroidSUT.debug = 4
        d = mozdevice.DroidSUT("127.0.0.1", port=port)
        thread.join()

    def test_err(self):
        """Tests error handling during initialization."""
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.bind(("127.0.0.1", 0))
        self._sock.listen(1)

        thread = self._serve([("testroot", "/mnt/sdcard"),
                              ("cd /mnt/sdcard/tests", "##AGENT-WARNING## no such file or directory"),
                              ("cd /mnt/sdcard/tests", "##AGENT-WARNING## no such file or directory"),
                              ("mkdr /mnt/sdcard/tests", "/mnt/sdcard/tests successfully created"),
                              ("ver", "SUTAgentAndroid Version XX")])

        port = self._sock.getsockname()[1]
        mozdevice.DroidSUT.debug = 4
        dm = mozdevice.DroidSUT("127.0.0.1", port=port)
        thread.join()

    def test_timeout_normal(self):
        """Tests DeviceManager timeout, normal case."""
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.bind(("127.0.0.1", 0))
        self._sock.listen(1)

        thread = self._serve([("testroot", "/mnt/sdcard"),
                              ("cd /mnt/sdcard/tests", ""),
                              ("cwd", "/mnt/sdcard/tests"),
                              ("ver", "SUTAgentAndroid Version XX"),
                              ("rm /mnt/sdcard/tests/test.txt", "Removed the file")])

        port = self._sock.getsockname()[1]
        mozdevice.DroidSUT.debug = 4
        d = mozdevice.DroidSUT("127.0.0.1", port=port)
        data = d.removeFile('/mnt/sdcard/tests/test.txt')
        self.assertEqual(data, "Removed the file")
        thread.join()

    def test_timeout_timeout(self):
        """Tests DeviceManager timeout, timeout case."""
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.bind(("127.0.0.1", 0))
        self._sock.listen(1)

        thread = self._serve([("testroot", "/mnt/sdcard"),
                              ("cd /mnt/sdcard/tests", ""),
                              ("cwd", "/mnt/sdcard/tests"),
                              ("ver", "SUTAgentAndroid Version XX"),
                              ("rm /mnt/sdcard/tests/test.txt", 3)])

        port = self._sock.getsockname()[1]
        mozdevice.DroidSUT.debug = 4
        d = mozdevice.DroidSUT("127.0.0.1", port=port)
        d.default_timeout = 1
        data = d.removeFile('/mnt/sdcard/tests/test.txt')
        self.assertEqual(data, None)
        thread.join()

if __name__ == '__main__':
    unittest.main()
