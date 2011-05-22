# Copyright 2011, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


"""Stream class for IETF HyBi 07 WebSocket protocol.
"""


from collections import deque
import os
import struct

from mod_pywebsocket import common
from mod_pywebsocket import util
from mod_pywebsocket._stream_base import BadOperationException
from mod_pywebsocket._stream_base import ConnectionTerminatedException
from mod_pywebsocket._stream_base import InvalidFrameException
from mod_pywebsocket._stream_base import StreamBase
from mod_pywebsocket._stream_base import UnsupportedFrameException


def is_control_opcode(opcode):
    return (opcode >> 3) == 1


_NOOP_MASKER = util.NoopMasker()


# Helper functions made public to be used for writing unittests for WebSocket
# clients.
def create_length_header(length, mask):
    """Creates a length header.

    Args:
        length: Frame length. Must be less than 2^63.
        mask: Mask bit. Must be boolean.

    Raises:
        ValueError: when bad data is given.
    """

    if mask:
        mask_bit = 1 << 7
    else:
        mask_bit = 0

    if length < 0:
        raise ValueError('length must be non negative integer')
    elif length <= 125:
        return chr(mask_bit | length)
    elif length < (1 << 16):
        return chr(mask_bit | 126) + struct.pack('!H', length)
    elif length < (1 << 63):
        return chr(mask_bit | 127) + struct.pack('!Q', length)
    else:
        raise ValueError('Payload is too big for one frame')


def create_header(opcode, payload_length, fin, rsv1, rsv2, rsv3, mask):
    """Creates a frame header.

    Raises:
        Exception: when bad data is given.
    """

    if opcode < 0 or 0xf < opcode:
        raise ValueError('Opcode out of range')

    if payload_length < 0 or (1 << 63) <= payload_length:
        raise ValueError('payload_length out of range')

    if (fin | rsv1 | rsv2 | rsv3) & ~1:
        raise ValueError('FIN bit and Reserved bit parameter must be 0 or 1')

    header = ''

    first_byte = ((fin << 7)
                  | (rsv1 << 6) | (rsv2 << 5) | (rsv3 << 4)
                  | opcode)
    header += chr(first_byte)
    header += create_length_header(payload_length, mask)

    return header


def _build_frame(header, body, mask):
    if not mask:
        return header + body

    masking_nonce = os.urandom(4)
    masker = util.RepeatedXorMasker(masking_nonce)

    return header + masking_nonce + masker.mask(body)


def create_text_frame(message, opcode=common.OPCODE_TEXT, fin=1, mask=False):
    """Creates a simple text frame with no extension, reserved bit."""

    encoded_message = message.encode('utf-8')
    header = create_header(opcode, len(encoded_message), fin, 0, 0, 0, mask)
    return _build_frame(header, encoded_message, mask)


class FragmentedTextFrameBuilder(object):
    """A stateful class to send a message as fragments."""

    def __init__(self, mask):
        """Constructs an instance."""

        self._mask = mask

        self._started = False

    def build(self, message, end):
        if self._started:
            opcode = common.OPCODE_CONTINUATION
        else:
            opcode = common.OPCODE_TEXT

        if end:
            self._started = False
            fin = 1
        else:
            self._started = True
            fin = 0

        return create_text_frame(message, opcode, fin, self._mask)


def create_ping_frame(body, mask=False):
    header = create_header(common.OPCODE_PING, len(body), 1, 0, 0, 0, mask)
    return _build_frame(header, body, mask)


def create_pong_frame(body, mask=False):
    header = create_header(common.OPCODE_PONG, len(body), 1, 0, 0, 0, mask)
    return _build_frame(header, body, mask)


def create_close_frame(body, mask=False):
    header = create_header(common.OPCODE_CLOSE, len(body), 1, 0, 0, 0, mask)
    return _build_frame(header, body, mask)


class StreamOptions(object):
    def __init__(self):
        self.deflate = False
        self.mask_send = False
        self.unmask_receive = True


class Stream(StreamBase):
    """Stream of WebSocket messages."""

    def __init__(self, request, options):
        """Constructs an instance.

        Args:
            request: mod_python request.
        """

        StreamBase.__init__(self, request)

        self._logger = util.get_class_logger(self)

        self._options = options

        if self._options.deflate:
            self._logger.debug('Deflated stream')
            self._request = util.DeflateRequest(self._request)

        self._request.client_terminated = False
        self._request.server_terminated = False

        # Holds body of received fragments.
        self._received_fragments = []
        # Holds the opcode of the first fragment.
        self._original_opcode = None

        self._writer = FragmentedTextFrameBuilder(self._options.mask_send)

        self._ping_queue = deque()

    def _receive_frame(self):
        """Receives a frame and return data in the frame as a tuple containing
        each header field and payload separately.

        Raises:
            ConnectionTerminatedException: when read returns empty
                string.
            InvalidFrameException: when the frame contains invalid data.
        """

        received = self.receive_bytes(2)

        first_byte = ord(received[0])
        fin = (first_byte >> 7) & 1
        rsv1 = (first_byte >> 6) & 1
        rsv2 = (first_byte >> 5) & 1
        rsv3 = (first_byte >> 4) & 1
        opcode = first_byte & 0xf

        second_byte = ord(received[1])
        mask = (second_byte >> 7) & 1
        payload_length = second_byte & 0x7f

        if (mask == 1) != self._options.unmask_receive:
            raise InvalidFrameException(
                'Mask bit on the received frame did\'nt match masking '
                'configuration for received frames')

        if payload_length == 127:
            extended_payload_length = self.receive_bytes(8)
            payload_length = struct.unpack(
                '!Q', extended_payload_length)[0]
            if payload_length > 0x7FFFFFFFFFFFFFFF:
                raise InvalidFrameException(
                    'Extended payload length >= 2^63')
        elif payload_length == 126:
            extended_payload_length = self.receive_bytes(2)
            payload_length = struct.unpack(
                '!H', extended_payload_length)[0]

        if mask == 1:
            masking_nonce = self.receive_bytes(4)
            masker = util.RepeatedXorMasker(masking_nonce)
        else:
            masker = _NOOP_MASKER

        bytes = masker.mask(self.receive_bytes(payload_length))

        return opcode, bytes, fin, rsv1, rsv2, rsv3

    def send_message(self, message, end=True):
        """Send message.

        Args:
            message: unicode string to send.

        Raises:
            BadOperationException: when called on a server-terminated
                connection.
        """

        if self._request.server_terminated:
            raise BadOperationException(
                'Requested send_message after sending out a closing handshake')

        self._write(self._writer.build(message, end))

    def receive_message(self):
        """Receive a WebSocket frame and return its payload an unicode string.

        Returns:
            payload unicode string in a WebSocket frame. None iff received
            closing handshake.
        Raises:
            BadOperationException: when called on a client-terminated
                connection.
            ConnectionTerminatedException: when read returns empty
                string.
            InvalidFrameException: when the frame contains invalid
                data.
            UnsupportedFrameException: when the received frame has
                flags, opcode we cannot handle. You can ignore this exception
                and continue receiving the next frame.
        """

        if self._request.client_terminated:
            raise BadOperationException(
                'Requested receive_message after receiving a closing handshake')

        while True:
            # mp_conn.read will block if no bytes are available.
            # Timeout is controlled by TimeOut directive of Apache.

            opcode, bytes, fin, rsv1, rsv2, rsv3 = self._receive_frame()
            if rsv1 or rsv2 or rsv3:
                raise UnsupportedFrameException(
                    'Unsupported flag is set (rsv = %d%d%d)' %
                    (rsv1, rsv2, rsv3))

            if opcode == common.OPCODE_CONTINUATION:
                if not self._received_fragments:
                    if fin:
                        raise InvalidFrameException(
                            'Received a termination frame but fragmentation '
                            'not started')
                    else:
                        raise InvalidFrameException(
                            'Received an intermediate frame but '
                            'fragmentation not started')

                if fin:
                    # End of fragmentation frame
                    self._received_fragments.append(bytes)
                    message = ''.join(self._received_fragments)
                    self._received_fragments = []
                else:
                    # Intermediate frame
                    self._received_fragments.append(bytes)
                    continue
            else:
                if self._received_fragments:
                    if fin:
                        raise InvalidFrameException(
                            'Received an unfragmented frame without '
                            'terminating existing fragmentation')
                    else:
                        raise InvalidFrameException(
                            'New fragmentation started without terminating '
                            'existing fragmentation')

                if fin:
                    # Unfragmented frame
                    self._original_opcode = opcode
                    message = bytes

                    if is_control_opcode(opcode) and len(message) > 125:
                        raise InvalidFrameException(
                            'Application data size of control frames must be '
                            '125 bytes or less')
                else:
                    # Start of fragmentation frame

                    if is_control_opcode(opcode):
                        raise InvalidFrameException(
                            'Control frames must not be fragmented')

                    self._original_opcode = opcode
                    self._received_fragments.append(bytes)
                    continue

            if self._original_opcode == common.OPCODE_TEXT:
                # The WebSocket protocol section 4.4 specifies that invalid
                # characters must be replaced with U+fffd REPLACEMENT
                # CHARACTER.
                return message.decode('utf-8', 'replace')
            elif self._original_opcode == common.OPCODE_CLOSE:
                self._request.client_terminated = True

                # Status code is optional. We can have status reason only if we
                # have status code. Status reason can be empty string. So,
                # allowed cases are
                # - no application data: no code no reason
                # - 2 octet of application data: has code but no reason
                # - 3 or more octet of application data: both code and reason
                if len(message) == 1:
                    raise InvalidFrameException(
                        'If a close frame has status code, the length of '
                        'status code must be 2 octet')
                elif len(message) >= 2:
                    self._request.ws_close_code = struct.unpack(
                        '!H', message[0:2])[0]
                    self._request.ws_close_reason = message[2:].decode(
                        'utf-8', 'replace')

                if self._request.server_terminated:
                    self._logger.debug(
                        'Received ack for server-initiated closing '
                        'handshake')
                    return None

                self._logger.debug(
                    'Received client-initiated closing handshake')

                self._send_closing_handshake(common.STATUS_NORMAL, '')
                self._logger.debug(
                    'Sent ack for client-initiated closing handshake')
                return None
            elif self._original_opcode == common.OPCODE_PING:
                try:
                    handler = self._request.on_ping_handler
                    if handler:
                        handler(self._request, message)
                        continue
                except AttributeError, e:
                    pass
                self._send_pong(message)
            elif self._original_opcode == common.OPCODE_PONG:
                # TODO(tyoshino): Add ping timeout handling.

                inflight_pings = deque()

                while True:
                    try:
                        expected_body = self._ping_queue.popleft()
                        if expected_body == message:
                            # inflight_pings contains pings ignored by the
                            # other peer. Just forget them.
                            self._logger.debug(
                                'Ping %r is acked (%d pings were ignored)' %
                                (expected_body, len(inflight_pings)))
                            break
                        else:
                            inflight_pings.append(expected_body)
                    except IndexError, e:
                        # The received pong was unsolicited pong. Keep the
                        # ping queue as is.
                        self._ping_queue = inflight_pings
                        self._logger.debug('Received a unsolicited pong')
                        break

                try:
                    handler = self._request.on_pong_handler
                    if handler:
                        handler(self._request, message)
                        continue
                except AttributeError, e:
                    pass

                continue
            else:
                raise UnsupportedFrameException(
                    'Opcode %d is not supported' % self._original_opcode)

    def _send_closing_handshake(self, code, reason):
        if code >= (1 << 16) or code < 0:
            raise BadOperationException('Status code is out of range')

        encoded_reason = reason.encode('utf-8')
        if len(encoded_reason) + 2 > 125:
            raise BadOperationException(
                'Application data size of close frames must be 125 bytes or '
                'less')

        frame = create_close_frame(
            struct.pack('!H', code) + encoded_reason, self._options.mask_send)

        self._request.server_terminated = True

        self._write(frame)

    def close_connection(self, code=common.STATUS_NORMAL, reason=''):
        """Closes a WebSocket connection."""

        if self._request.server_terminated:
            self._logger.debug(
                'Requested close_connection but server is already terminated')
            return

        self._send_closing_handshake(code, reason)
        self._logger.debug('Sent server-initiated closing handshake')

        if (code == common.STATUS_GOING_AWAY or
            code == common.STATUS_PROTOCOL_ERROR):
            # It doesn't make sense to wait for a close frame if the reason is
            # protocol error or that the server is going away. For some of other
            # reasons, it might not make sense to wait for a close frame, but
            # it's not clear, yet.
            return

        # TODO(ukai): 2. wait until the /client terminated/ flag has been set,
        # or until a server-defined timeout expires.
        #
        # For now, we expect receiving closing handshake right after sending
        # out closing handshake.
        message = self.receive_message()
        if message is not None:
            raise ConnectionTerminatedException(
                'Didn\'t receive valid ack for closing handshake')
        # TODO: 3. close the WebSocket connection.
        # note: mod_python Connection (mp_conn) doesn't have close method.

    def send_ping(self, body=''):
        if len(body) > 125:
            raise ValueError(
                'Application data size of control frames must be 125 bytes or '
                'less')
        frame = create_ping_frame(body, self._options.mask_send)
        self._write(frame)

        self._ping_queue.append(body)

    def _send_pong(self, body):
        if len(body) > 125:
            raise ValueError(
                'Application data size of control frames must be 125 bytes or '
                'less')
        frame = create_pong_frame(body, self._options.mask_send)
        self._write(frame)


# vi:sts=4 sw=4 et
