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


"""WebSocket initial handshake hander for HyBi 00 protocol."""


# Note: request.connection.write/read are used in this module, even though
# mod_python document says that they should be used only in connection
# handlers. Unfortunately, we have no other options. For example,
# request.write/read are not suitable because they don't allow direct raw bytes
# writing/reading.


import logging
import re
import struct

from mod_pywebsocket import common
from mod_pywebsocket.stream import StreamHixie75
from mod_pywebsocket import util
from mod_pywebsocket.handshake._base import HandshakeError
from mod_pywebsocket.handshake._base import build_location
from mod_pywebsocket.handshake._base import check_header_lines
from mod_pywebsocket.handshake._base import get_mandatory_header
from mod_pywebsocket.handshake._base import validate_subprotocol


_MANDATORY_HEADERS = [
    # key, expected value or None
    [common.UPGRADE_HEADER, common.WEBSOCKET_UPGRADE_TYPE_HIXIE75],
    [common.CONNECTION_HEADER, common.UPGRADE_CONNECTION_TYPE],
]


class Handshaker(object):
    """This class performs WebSocket handshake."""

    def __init__(self, request, dispatcher):
        """Construct an instance.

        Args:
            request: mod_python request.
            dispatcher: Dispatcher (dispatch.Dispatcher).

        Handshaker will add attributes such as ws_resource in performing
        handshake.
        """

        self._logger = util.get_class_logger(self)

        self._request = request
        self._dispatcher = dispatcher

    def do_handshake(self):
        """Perform WebSocket Handshake.

        On _request, we set
            ws_resource, ws_protocol, ws_location, ws_origin, ws_challenge,
            ws_challenge_md5: WebSocket handshake information.
            ws_stream: Frame generation/parsing class.
            ws_version: Protocol version.
        """

        # 5.1 Reading the client's opening handshake.
        # dispatcher sets it in self._request.
        check_header_lines(self._request, _MANDATORY_HEADERS)
        self._set_resource()
        self._set_subprotocol()
        self._set_location()
        self._set_origin()
        self._set_challenge_response()
        self._set_protocol_version()
        self._dispatcher.do_extra_handshake(self._request)
        self._send_handshake()

    def _set_resource(self):
        self._request.ws_resource = self._request.uri

    def _set_subprotocol(self):
        # |Sec-WebSocket-Protocol|
        subprotocol = self._request.headers_in.get(
            common.SEC_WEBSOCKET_PROTOCOL_HEADER)
        if subprotocol is not None:
            validate_subprotocol(subprotocol)
        self._request.ws_protocol = subprotocol

    def _set_location(self):
        # |Host|
        host = self._request.headers_in.get(common.HOST_HEADER)
        if host is not None:
            self._request.ws_location = build_location(self._request)
        # TODO(ukai): check host is this host.

    def _set_origin(self):
        # |Origin|
        origin = self._request.headers_in['Origin']
        if origin is not None:
            self._request.ws_origin = origin

    def _set_protocol_version(self):
        # |Sec-WebSocket-Draft|
        draft = self._request.headers_in.get('Sec-WebSocket-Draft')
        if draft is not None:
            try:
                draft_int = int(draft)

                # Draft value 2 is used by HyBi 02 and 03 which we no longer
                # support. draft >= 3 and <= 1 are never defined in the spec.
                # 0 might be used to mean HyBi 00 by somebody. 1 might be used
                # to mean HyBi 01 by somebody but we no longer support it.

                if draft_int == 1 or draft_int == 2:
                    raise HandshakeError('HyBi 01-03 are not supported')
                elif draft_int != 0:
                    raise ValueError
            except ValueError, e:
                raise HandshakeError(
                    'Illegal value for Sec-WebSocket-Draft: %s' % draft)

        self._logger.debug('IETF HyBi 00 protocol')
        self._request.ws_version = common.VERSION_HYBI00
        self._request.ws_stream = StreamHixie75(self._request, True)

    def _set_challenge_response(self):
        # 5.2 4-8.
        self._request.ws_challenge = self._get_challenge()
        # 5.2 9. let /response/ be the MD5 finterprint of /challenge/
        self._request.ws_challenge_md5 = util.md5_hash(
            self._request.ws_challenge).digest()
        self._logger.debug(
            'Challenge: %r (%s)' %
            (self._request.ws_challenge,
             util.hexify(self._request.ws_challenge)))
        self._logger.debug(
            'Challenge response: %r (%s)' %
            (self._request.ws_challenge_md5,
             util.hexify(self._request.ws_challenge_md5)))

    def _get_key_value(self, key_field):
        key_value = get_mandatory_header(self._request, key_field)

        # 5.2 4. let /key-number_n/ be the digits (characters in the range
        # U+0030 DIGIT ZERO (0) to U+0039 DIGIT NINE (9)) in /key_n/,
        # interpreted as a base ten integer, ignoring all other characters
        # in /key_n/.
        try:
            key_number = int(re.sub("\\D", "", key_value))
        except:
            raise HandshakeError('%s field contains no digit' % key_field)
        # 5.2 5. let /spaces_n/ be the number of U+0020 SPACE characters
        # in /key_n/.
        spaces = re.subn(" ", "", key_value)[1]
        if spaces == 0:
            raise HandshakeError('%s field contains no space' % key_field)
        # 5.2 6. if /key-number_n/ is not an integral multiple of /spaces_n/
        # then abort the WebSocket connection.
        if key_number % spaces != 0:
            raise HandshakeError('Key-number %d is not an integral '
                                 'multiple of spaces %d' % (key_number,
                                                            spaces))
        # 5.2 7. let /part_n/ be /key-number_n/ divided by /spaces_n/.
        part = key_number / spaces
        self._logger.debug('%s: %s => %d / %d => %d' % (
            key_field, key_value, key_number, spaces, part))
        return part

    def _get_challenge(self):
        # 5.2 4-7.
        key1 = self._get_key_value('Sec-WebSocket-Key1')
        key2 = self._get_key_value('Sec-WebSocket-Key2')
        # 5.2 8. let /challenge/ be the concatenation of /part_1/,
        challenge = ''
        challenge += struct.pack('!I', key1)  # network byteorder int
        challenge += struct.pack('!I', key2)  # network byteorder int
        challenge += self._request.connection.read(8)
        return challenge

    def _sendall(self, data):
        self._request.connection.write(data)

    def _send_header(self, name, value):
        self._sendall('%s: %s\r\n' % (name, value))

    def _send_handshake(self):
        # 5.2 10. send the following line.
        self._sendall('HTTP/1.1 101 WebSocket Protocol Handshake\r\n')
        # 5.2 11. send the following fields to the client.
        self._send_header(
            common.UPGRADE_HEADER, common.WEBSOCKET_UPGRADE_TYPE_HIXIE75)
        self._send_header(
            common.CONNECTION_HEADER, common.UPGRADE_CONNECTION_TYPE)
        self._send_header('Sec-WebSocket-Location', self._request.ws_location)
        self._send_header(
            common.SEC_WEBSOCKET_ORIGIN_HEADER, self._request.ws_origin)
        if self._request.ws_protocol:
            self._send_header(
                common.SEC_WEBSOCKET_PROTOCOL_HEADER,
                self._request.ws_protocol)
        # 5.2 12. send two bytes 0x0D 0x0A.
        self._sendall('\r\n')
        # 5.2 13. send /response/
        self._sendall(self._request.ws_challenge_md5)


# vi:sts=4 sw=4 et
