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


"""WebSocket opening handshake processor. This class try to apply available
opening handshake processors for each protocol version until a connection is
successfully established.
"""


import logging

from mod_pywebsocket import util
from mod_pywebsocket.handshake import draft75
from mod_pywebsocket.handshake import hybi00
from mod_pywebsocket.handshake import hybi06
# Export Extension symbol from this module.
from mod_pywebsocket.handshake._base import Extension
# Export HandshakeError symbol from this module.
from mod_pywebsocket.handshake._base import HandshakeError


class Handshaker(object):
    """This class performs WebSocket handshake."""

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

        self._logger = util.get_class_logger(self)

        self._request = request
        self._dispatcher = dispatcher
        self._strict = strict
        self._hybi07Handshaker = hybi06.Handshaker(request, dispatcher)
        self._hybi00Handshaker = hybi00.Handshaker(request, dispatcher)
        self._hixie75Handshaker = None
        if allowDraft75:
            self._hixie75Handshaker = draft75.Handshaker(
                request, dispatcher, strict)

    def do_handshake(self):
        """Perform WebSocket Handshake."""

        self._logger.debug('Opening handshake resource: %r', self._request.uri)
        # To print mimetools.Message as escaped one-line string, we converts
        # headers_in to dict object. Without conversion, if we use %r, it just
        # prints the type and address, and if we use %s, it prints the original
        # header string as multiple lines.
        #
        # Both mimetools.Message and MpTable_Type of mod_python can be
        # converted to dict.
        #
        # mimetools.Message.__str__ returns the original header string.
        # dict(mimetools.Message object) returns the map from header names to
        # header values. While MpTable_Type doesn't have such __str__ but just
        # __repr__ which formats itself as well as dictionary object.
        self._logger.debug(
            'Opening handshake request headers: %r',
            dict(self._request.headers_in))

        handshakers = [
            ('HyBi 07', self._hybi07Handshaker),
            ('HyBi 00', self._hybi00Handshaker),
            ('Hixie 75', self._hixie75Handshaker)]
        last_error = HandshakeError('No handshaker available')
        for name, handshaker in handshakers:
            if handshaker:
                self._logger.info('Trying %s protocol', name)
                try:
                    handshaker.do_handshake()
                    return
                except HandshakeError, e:
                    self._logger.info('%s handshake failed: %s', name, e)
                    last_error = e
        raise last_error


# vi:sts=4 sw=4 et
