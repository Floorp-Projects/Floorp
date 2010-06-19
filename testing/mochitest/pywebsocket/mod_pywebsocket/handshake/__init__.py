# Copyright 2010, Google Inc.
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
import re

from mod_pywebsocket.handshake import draft75
from mod_pywebsocket.handshake import handshake
from mod_pywebsocket.handshake._base import DEFAULT_WEB_SOCKET_PORT
from mod_pywebsocket.handshake._base import DEFAULT_WEB_SOCKET_SECURE_PORT
from mod_pywebsocket.handshake._base import WEB_SOCKET_SCHEME
from mod_pywebsocket.handshake._base import WEB_SOCKET_SECURE_SCHEME
from mod_pywebsocket.handshake._base import HandshakeError
from mod_pywebsocket.handshake._base import validate_protocol


class Handshaker(object):
    """This class performs Web Socket handshake."""

    def __init__(self, request, dispatcher, allowDraft75=False, strict=False):
        """Construct an instance.

        Args:
            request: mod_python request.
            dispatcher: Dispatcher (dispatch.Dispatcher).
            allowDraft75: allow draft 75 handshake protocol.
            strict: Strictly check handshake request in draft 75.
                Default: False. If True, request.connection must provide
                get_memorized_lines method.

        Handshaker will add attributes such as ws_resource in performing
        handshake.
        """

        self._logger = logging.getLogger("mod_pywebsocket.handshake")
        self._request = request
        self._dispatcher = dispatcher
        self._strict = strict
        self._handshaker = handshake.Handshaker(request, dispatcher)
        self._fallbackHandshaker = None
        if allowDraft75:
            self._fallbackHandshaker = draft75.Handshaker(
                request, dispatcher, strict)

    def do_handshake(self):
        """Perform Web Socket Handshake."""

        try:
            self._handshaker.do_handshake()
        except HandshakeError, e:
            self._logger.error('Handshake error: %s' % e)
            if self._fallbackHandshaker:
                self._logger.warning('fallback to old protocol')
                self._fallbackHandshaker.do_handshake()
                return
            raise e


# vi:sts=4 sw=4 et
