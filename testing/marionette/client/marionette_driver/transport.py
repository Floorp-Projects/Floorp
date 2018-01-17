# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import json
import socket
import sys
import time

from six import reraise


class SocketTimeout(object):
    def __init__(self, socket, timeout):
        self.sock = socket
        self.timeout = timeout
        self.old_timeout = None

    def __enter__(self):
        self.old_timeout = self.sock.gettimeout()
        self.sock.settimeout(self.timeout)

    def __exit__(self, *args, **kwargs):
        self.sock.settimeout(self.old_timeout)


class Message(object):
    def __init__(self, msgid):
        self.id = msgid

    def __eq__(self, other):
        return self.id == other.id

    def __ne__(self, other):
        return not self.__eq__(other)


class Command(Message):
    TYPE = 0

    def __init__(self, msgid, name, params):
        Message.__init__(self, msgid)
        self.name = name
        self.params = params

    def __str__(self):
        return "<Command id={0}, name={1}, params={2}>".format(self.id, self.name, self.params)

    def to_msg(self):
        msg = [Command.TYPE, self.id, self.name, self.params]
        return json.dumps(msg)

    @staticmethod
    def from_msg(payload):
        data = json.loads(payload)
        assert data[0] == Command.TYPE
        cmd = Command(data[1], data[2], data[3])
        return cmd


class Response(Message):
    TYPE = 1

    def __init__(self, msgid, error, result):
        Message.__init__(self, msgid)
        self.error = error
        self.result = result

    def __str__(self):
        return "<Response id={0}, error={1}, result={2}>".format(self.id, self.error, self.result)

    def to_msg(self):
        msg = [Response.TYPE, self.id, self.error, self.result]
        return json.dumps(msg)

    @staticmethod
    def from_msg(payload):
        data = json.loads(payload)
        assert data[0] == Response.TYPE
        return Response(data[1], data[2], data[3])


class TcpTransport(object):
    """Socket client that communciates with Marionette via TCP.

    It speaks the protocol of the remote debugger in Gecko, in which
    messages are always preceded by the message length and a colon, e.g.:

        7:MESSAGE

    On top of this protocol it uses a Marionette message format, that
    depending on the protocol level offered by the remote server, varies.
    Supported protocol levels are `min_protocol_level` and above.
    """
    max_packet_length = 4096
    min_protocol_level = 3

    def __init__(self, host, port, socket_timeout=60.0):
        """If `socket_timeout` is `0` or `0.0`, non-blocking socket mode
        will be used.  Setting it to `1` or `None` disables timeouts on
        socket operations altogether.
        """
        self._sock = None

        self.host = host
        self.port = port
        self.socket_timeout = socket_timeout

        self.protocol = self.min_protocol_level
        self.application_type = None
        self.last_id = 0
        self.expected_response = None

    @property
    def socket_timeout(self):
        return self._socket_timeout

    @socket_timeout.setter
    def socket_timeout(self, value):
        self._socket_timeout = value

        if self._sock:
            self._sock.settimeout(value)

    def _unmarshal(self, packet):
        msg = None

        # protocol 3 and above
        if self.protocol >= 3:
            typ = int(packet[1])
            if typ == Command.TYPE:
                msg = Command.from_msg(packet)
            elif typ == Response.TYPE:
                msg = Response.from_msg(packet)

        return msg

    def receive(self, unmarshal=True):
        """Wait for the next complete response from the remote.

        :param unmarshal: Default is to deserialise the packet and
            return a ``Message`` type.  Setting this to false will return
            the raw packet.
        """
        now = time.time()
        data = ""
        bytes_to_recv = 10

        while self.socket_timeout is None or (time.time() - now < self.socket_timeout):
            try:
                chunk = self._sock.recv(bytes_to_recv)
                data += chunk
            except socket.timeout:
                pass
            else:
                if not chunk:
                    raise socket.error("No data received over socket")

            sep = data.find(":")
            if sep > -1:
                length = data[0:sep]
                remaining = data[sep + 1:]

                if len(remaining) == int(length):
                    if unmarshal:
                        msg = self._unmarshal(remaining)
                        self.last_id = msg.id

                        # keep reading incoming responses until
                        # we receive the user's expected response
                        if isinstance(msg, Response) and msg != self.expected_response:
                            return self.receive(unmarshal)

                        return msg

                    else:
                        return remaining

                bytes_to_recv = int(length) - len(remaining)

        raise socket.timeout("Connection timed out after {}s".format(self.socket_timeout))

    def connect(self):
        """Connect to the server and process the hello message we expect
        to receive in response.

        Returns a tuple of the protocol level and the application type.
        """
        try:
            self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self._sock.settimeout(self.socket_timeout)

            self._sock.connect((self.host, self.port))
        except:
            # Unset so that the next attempt to send will cause
            # another connection attempt.
            self._sock = None
            raise

        try:
            with SocketTimeout(self._sock, 60.0):
                # first packet is always a JSON Object
                # which we can use to tell which protocol level we are at
                raw = self.receive(unmarshal=False)
        except socket.timeout:
            msg = "Connection attempt failed because no data has been received over the socket: {}"
            exc, val, tb = sys.exc_info()

            reraise(exc, msg.format(val), tb)

        hello = json.loads(raw)
        application_type = hello.get("applicationType")
        protocol = hello.get("marionetteProtocol")

        if application_type != "gecko":
            raise ValueError("Application type '{}' is not supported".format(application_type))

        if not isinstance(protocol, int) or protocol < self.min_protocol_level:
            msg = "Earliest supported protocol level is '{}' but got '{}'"
            raise ValueError(msg.format(self.min_protocol_level, protocol))

        self.application_type = application_type
        self.protocol = protocol

        return (self.protocol, self.application_type)

    def send(self, obj):
        """Send message to the remote server.  Allowed input is a
        ``Message`` instance or a JSON serialisable object.
        """
        if not self._sock:
            self.connect()

        if isinstance(obj, Message):
            data = obj.to_msg()
            if isinstance(obj, Command):
                self.expected_response = obj
        else:
            data = json.dumps(obj)
        payload = "{0}:{1}".format(len(data), data)

        totalsent = 0
        while totalsent < len(payload):
            sent = self._sock.send(payload[totalsent:])
            if sent == 0:
                raise IOError("Socket error after sending {0} of {1} bytes"
                              .format(totalsent, len(payload)))
            else:
                totalsent += sent

    def respond(self, obj):
        """Send a response to a command.  This can be an arbitrary JSON
        serialisable object or an ``Exception``.
        """
        res, err = None, None
        if isinstance(obj, Exception):
            err = obj
        else:
            res = obj
        msg = Response(self.last_id, err, res)
        self.send(msg)
        return self.receive()

    def request(self, name, params):
        """Sends a message to the remote server and waits for a response
        to come back.
        """
        self.last_id = self.last_id + 1
        cmd = Command(self.last_id, name, params)
        self.send(cmd)
        return self.receive()

    def close(self):
        """Close the socket.

        First forces the socket to not send data anymore, and then explicitly
        close it to free up its resources.

        See: https://docs.python.org/2/howto/sockets.html#disconnecting
        """
        if self._sock:
            try:
                self._sock.shutdown(socket.SHUT_RDWR)
            except IOError as exc:
                # If the socket is already closed, don't care about:
                #   Errno  57: Socket not connected
                #   Errno 107: Transport endpoint is not connected
                if exc.errno not in (57, 107):
                    raise

            self._sock.close()
            self._sock = None

    def __del__(self):
        self.close()
