# Copyright 2009, Google Inc.
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


"""Web Socket handshaking.

Note: request.connection.write/read are used in this module, even though
mod_python document says that they should be used only in connection handlers.
Unfortunately, we have no other options. For example, request.write/read are
not suitable because they don't allow direct raw bytes writing/reading.
"""


import logging
from md5 import md5
import re
import struct

from mod_pywebsocket.handshake._base import HandshakeError
from mod_pywebsocket.handshake._base import build_location
from mod_pywebsocket.handshake._base import validate_protocol


_MANDATORY_HEADERS = [
    # key, expected value or None
    ['Upgrade', 'WebSocket'],
    ['Connection', 'Upgrade'],
]

def _hexify(s):
    return re.sub('.', lambda x: '%02x ' % ord(x.group(0)), s)

class Handshaker(object):
    """This class performs Web Socket handshake."""

    def __init__(self, request, dispatcher):
        """Construct an instance.

        Args:
            request: mod_python request.
            dispatcher: Dispatcher (dispatch.Dispatcher).

        Handshaker will add attributes such as ws_resource in performing
        handshake.
        """

        self._logger = logging.getLogger("mod_pywebsocket.handshake")
        self._request = request
        self._dispatcher = dispatcher

    def do_handshake(self):
        """Perform Web Socket Handshake."""

        # 5.1 Reading the client's opening handshake.
        # dispatcher sets it in self._request.
        self._check_header_lines()
        self._set_resource()
        self._set_protocol()
        self._set_location()
        self._set_origin()
        self._set_challenge_response()
        self._dispatcher.do_extra_handshake(self._request)
        self._send_handshake()

    def _check_header_lines(self):
        # 5.1 1. The three character UTF-8 string "GET".
        # 5.1 2. A UTF-8-encoded U+0020 SPACE character (0x20 byte).
        if self._request.method != 'GET':
            raise HandshakeError('Method is not GET')
        # The expected field names, and the meaning of their corresponding
        # values, are as follows.
        #  |Upgrade| and |Connection|
        for key, expected_value in _MANDATORY_HEADERS:
            actual_value = self._request.headers_in.get(key)
            if not actual_value:
                raise HandshakeError('Header %s is not defined' % key)
            if expected_value:
                if actual_value != expected_value:
                    raise HandshakeError('Illegal value for header %s: %s' %
                                         (key, actual_value))

    def _set_resource(self):
        self._request.ws_resource = self._request.uri

    def _set_protocol(self):
        # |Sec-WebSocket-Protocol|
        protocol = self._request.headers_in.get('Sec-WebSocket-Protocol')
        if protocol is not None:
            validate_protocol(protocol)
        self._request.ws_protocol = protocol

    def _set_location(self):
        # |Host|
        host = self._request.headers_in.get('Host')
        if host is not None:
            self._request.ws_location = build_location(self._request)
        # TODO(ukai): check host is this host.

    def _set_origin(self):
        # |Origin|
        origin = self._request.headers_in['Origin']
        if origin is not None:
            self._request.ws_origin = origin

    def _set_challenge_response(self):
        # 5.2 4-8.
        self._request.ws_challenge = self._get_challenge()
        # 5.2 9. let /response/ be the MD5 finterprint of /challenge/
        self._request.ws_challenge_md5 = md5(
            self._request.ws_challenge).digest()
        self._logger.debug("challenge: %s" % _hexify(
            self._request.ws_challenge))
        self._logger.debug("response:  %s" % _hexify(
            self._request.ws_challenge_md5))

    def _get_key_value(self, key_field):
        key_value = self._request.headers_in.get(key_field)
        if key_value is None:
            self._logger.debug("no %s" % key_value)
            return None
        try:
            # 5.2 4. let /key-number_n/ be the digits (characters in the range
            # U+0030 DIGIT ZERO (0) to U+0039 DIGIT NINE (9)) in /key_n/,
            # interpreted as a base ten integer, ignoring all other characters
            # in /key_n/
            key_number = int(re.sub("\\D", "", key_value))
            # 5.2 5. let /spaces_n/ be the number of U+0020 SPACE characters
            # in /key_n/.
            spaces = re.subn(" ", "", key_value)[1]
            # 5.2 6. if /key-number_n/ is not an integral multiple of /spaces_n/
            # then abort the WebSocket connection.
            if key_number % spaces != 0:
                raise handshakeError('key_number %d is not an integral '
                                     'multiple of spaces %d' % (key_number,
                                                                spaces))
            # 5.2 7. let /part_n/ be /key_number_n/ divided by /spaces_n/.
            part = key_number / spaces
            self._logger.debug("%s: %s => %d / %d => %d" % (
                key_field, key_value, key_number, spaces, part))
            return part
        except:
            return None

    def _get_challenge(self):
        # 5.2 4-7.
        key1 = self._get_key_value('Sec-Websocket-Key1')
        if not key1:
            raise HandshakeError('Sec-WebSocket-Key1 not found')
        key2 = self._get_key_value('Sec-Websocket-Key2')
        if not key2:
            raise HandshakeError('Sec-WebSocket-Key2 not found')
        # 5.2 8. let /challenge/ be the concatenation of /part_1/,
        challenge = ""
        challenge += struct.pack("!I", key1)  # network byteorder int
        challenge += struct.pack("!I", key2)  # network byteorder int
        challenge += self._request.connection.read(8)
        return challenge

    def _send_handshake(self):
        # 5.2 10. send the following line.
        self._request.connection.write(
                'HTTP/1.1 101 WebSocket Protocol Handshake\r\n')
        # 5.2 11. send the following fields to the client.
        self._request.connection.write('Upgrade: WebSocket\r\n')
        self._request.connection.write('Connection: Upgrade\r\n')
        self._request.connection.write('Sec-WebSocket-Location: ')
        self._request.connection.write(self._request.ws_location)
        self._request.connection.write('\r\n')
        self._request.connection.write('Sec-WebSocket-Origin: ')
        self._request.connection.write(self._request.ws_origin)
        self._request.connection.write('\r\n')
        if self._request.ws_protocol:
            self._request.connection.write('Sec-WebSocket-Protocol: ')
            self._request.connection.write(self._request.ws_protocol)
            self._request.connection.write('\r\n')
        # 5.2 12. send two bytes 0x0D 0x0A.
        self._request.connection.write('\r\n')
        # 5.2 13. send /response/
        self._request.connection.write(self._request.ws_challenge_md5)


# vi:sts=4 sw=4 et
