#!/usr/bin/env python

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import datetime
import socket
import time

from threading import Thread

class MockAgent(object):

    MAX_WAIT_TIME_SECONDS = 10
    SOCKET_TIMEOUT_SECONDS = 5

    def __init__(self, tester, start_commands = None, commands = []):
        if start_commands:
            self.commands = start_commands
        else:
            self.commands = [("testroot", "/mnt/sdcard"),
                             ("isdir /mnt/sdcard/tests", "TRUE"),
                             ("ver", "SUTAgentAndroid Version 1.14")]
        self.commands = self.commands + commands

        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.bind(("127.0.0.1", 0))
        self._sock.listen(1)

        self.tester = tester

        self.thread = Thread(target=self._serve_thread)
        self.thread.start()

        self.should_stop = False

    @property
    def port(self):
        return self._sock.getsockname()[1]

    def _serve_thread(self):
        conn = None
        while self.commands:
            if not conn:
                conn, addr = self._sock.accept()
                conn.settimeout(self.SOCKET_TIMEOUT_SECONDS)
                conn.send("$>\x00")
            (command, response) = self.commands.pop(0)
            data = ''
            timeout = datetime.datetime.now() + datetime.timedelta(
                seconds=self.MAX_WAIT_TIME_SECONDS)
            # The data might come in chunks, particularly if we are expecting
            # multiple lines, as with push commands.
            while (len(data) < len(command) and
                   datetime.datetime.now() < timeout):
                try:
                    data += conn.recv(1024)
                except socket.timeout:
                    # We handle timeouts in the main loop.
                    pass
            self.tester.assertEqual(data.strip(), command)
            # send response and prompt separately to test for bug 789496
            # FIXME: Improve the mock agent, since overloading the meaning
            # of 'response' is getting confusing.
            if response is None: # code for "shut down"
                conn.shutdown(socket.SHUT_RDWR)
                conn.close()
                conn = None
            elif type(response) is int: # code for "time out"
                max_timeout = 15.0
                timeout = 0.0
                interval = 0.1
                while not self.should_stop and timeout < max_timeout:
                    time.sleep(interval)
                    timeout += interval
                if timeout >= max_timeout:
                    raise Exception("Maximum timeout reached! This should not "
                                    "happen")
                return
            else:
                # pull is handled specially, as we just pass back the full
                # command line
                if "pull" in command:
                    conn.send(response)
                else:
                    conn.send("%s\n" % response)
                conn.send("$>\x00")

    def wait(self):
        self.thread.join()
