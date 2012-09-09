#!/usr/bin/env python

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import socket
import mozdevice
from threading import Thread
import unittest

class BasicTest(unittest.TestCase):

    def _serve_thread(self):
        need_connection = True
        while self.commands:
            (command, response) = self.commands.pop(0)
            if need_connection:
                conn, addr = self._sock.accept()
                need_connection = False
            conn.send("$>\x00")
            data = conn.recv(1024).strip()
            self.assertEqual(data, command)
            conn.send("%s\n" % response)
            conn.send("$>\x00")

    def _serve(self, commands):
        self.commands = commands
        thread = Thread(target=self._serve_thread)
        thread.start()
        return thread

    def test_init(self):
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.bind(("127.0.0.1", 0))
        self._sock.listen(1)

        thread = self._serve([("testroot", "/mnt/sdcard"),
                              ("cd /mnt/sdcard/tests", ""),
                              ("cwd", "/mnt/sdcard/tests")])

        port = self._sock.getsockname()[1]
        d = mozdevice.DroidSUT("127.0.0.1", port=port)
        thread.join()

if __name__ == '__main__':
    unittest.main()
