# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import datetime
import errno
import json
import socket
import time


class MarionetteTransport(object):
    """The Marionette socket client.  This speaks the same protocol
    as the remote debugger inside Gecko, in which messages are always
    preceded by the message length and a colon, e.g.:

        20:{"command": "test"}
    """

    max_packet_length = 4096
    connection_lost_msg = "Connection to Marionette server is lost. Check gecko.log (desktop firefox) or logcat (b2g) for errors."

    def __init__(self, addr, port, socket_timeout=360.0):
        self.addr = addr
        self.port = port
        self.socket_timeout = socket_timeout
        self.sock = None
        self.protocol = 1
        self.application_type = None

    def _recv_n_bytes(self, n):
        """Convenience method for receiving exactly n bytes from self.sock
        (assuming it's open and connected).
        """
        data = ""
        while len(data) < n:
            chunk = self.sock.recv(n - len(data))
            if chunk == "":
                break
            data += chunk
        return data

    def receive(self):
        """Receive the next complete response from the server, and
        return it as a JSON structure.  Each response from the server
        is prepended by len(message) + ":".
        """
        assert(self.sock)
        now = time.time()
        response = ''
        bytes_to_recv = 10
        while time.time() - now < self.socket_timeout:
            try:
                data = self.sock.recv(bytes_to_recv)
                response += data
            except socket.timeout:
                pass
            else:
                if not data:
                    raise IOError(self.connection_lost_msg)
            sep = response.find(':')
            if sep > -1:
                length = response[0:sep]
                remaining = response[sep + 1:]
                if len(remaining) == int(length):
                    return json.loads(remaining)
                bytes_to_recv = int(length) - len(remaining)
        raise socket.timeout('connection timed out after %d s' % self.socket_timeout)

    def connect(self):
        """Connect to the server and process the hello message we expect
        to receive in response.

        Return a tuple of the protocol level and the application type.
        """
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(self.socket_timeout)
        try:
            self.sock.connect((self.addr, self.port))
        except:
            # Unset self.sock so that the next attempt to send will cause
            # another connection attempt.
            self.sock = None
            raise
        self.sock.settimeout(2.0)

        hello = self.receive()
        self.protocol = hello.get("marionetteProtocol", 1)
        self.application_type = hello.get("applicationType")

        return (self.protocol, self.application_type)

    def send(self, data):
        """Send a message on the socket, prepending it with len(msg) + ":"."""
        if not self.sock:
            self.connect()
        data = "%s:%s" % (len(data), data)

        for packet in [data[i:i + self.max_packet_length] for i in
                       range(0, len(data), self.max_packet_length)]:
            try:
                self.sock.send(packet)
            except IOError as e:
                if e.errno == errno.EPIPE:
                    raise IOError("%s: %s" % (str(e), self.connection_lost_msg))
                else:
                    raise e

        return self.receive()

    def close(self):
        """Close the socket."""
        if self.sock:
            self.sock.close()
        self.sock = None

    @staticmethod
    def wait_for_port(host, port, timeout=60):
        """ Wait for the specified Marionette host/port to be available."""
        starttime = datetime.datetime.now()
        poll_interval = 0.1
        while datetime.datetime.now() - starttime < datetime.timedelta(seconds=timeout):
            sock = None
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.connect((host, port))
                data = sock.recv(16)
                sock.close()
                if ':' in data:
                    return True
            except socket.error:
                pass
            finally:
                if sock:
                    sock.close()
            time.sleep(poll_interval)
        return False
