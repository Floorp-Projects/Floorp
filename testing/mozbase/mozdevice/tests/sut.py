#!/usr/bin/env python

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import socket
import mozdevice
from threading import Thread
import unittest

class BasicTest(unittest.TestCase):

    def _serve_thread(self):
        conn, addr = self._sock.accept()
        conn.send("$>\x00")
        while self.commands:
            (command, response) = self.commands.pop(0)
            data = conn.recv(1024).strip()
            self.assertEqual(data, command)
            # send response and prompt separately to test for bug 789496
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

if __name__ == '__main__':
    unittest.main()
