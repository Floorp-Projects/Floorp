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


"""Common functions and exceptions used by WebSocket opening handshake
processors.
"""


from mod_pywebsocket import common
from mod_pywebsocket import http_header_util


class Extension(object):
    """Holds information about an extension which is exchanged on extension
    negotiation in opening handshake.
    """

    def __init__(self, name):
        self._name = name
        # TODO(tyoshino): Change the data structure to more efficient one such
        # as dict when the spec changes to say like
        # - Parameter names must be unique
        # - The order of parameters is not significant
        self._parameters = []

    def name(self):
        return self._name

    def add_parameter(self, name, value):
        self._parameters.append((name, value))

    def get_parameter(self, name):
        for param_name, param_value in self._parameters:
            if param_name == name:
                return param_value

    def get_parameter_names(self):
        return [name for name, unused_value in self._parameters]

    def get_formatted_string(self):
        formatted_params = [self._name]
        for param_name, param_value in self._parameters:
            if param_value is None:
                formatted_params.append(param_name)
            else:
                quoted_value = http_header_util.quote_if_necessary(param_value)
                formatted_params.append('%s=%s' % (param_name, quoted_value))

        return '; '.join(formatted_params)


class HandshakeError(Exception):
    """This exception will be raised when an error occurred while processing
    WebSocket initial handshake.
    """
    pass


def get_default_port(is_secure):
    if is_secure:
        return common.DEFAULT_WEB_SOCKET_SECURE_PORT
    else:
        return common.DEFAULT_WEB_SOCKET_PORT


# TODO(tyoshino): Have stricter validator for HyBi 07.
def validate_subprotocol(subprotocol):
    """Validate a value in subprotocol fields such as WebSocket-Protocol,
    Sec-WebSocket-Protocol.

    See
    - HyBi 06: Section 5.2.2.
    - HyBi 00: Section 4.1. Opening handshake
    - Hixie 75: Section 4.1. Handshake
    """

    if not subprotocol:
        raise HandshakeError('Invalid subprotocol name: empty')
    for c in subprotocol:
        if not 0x20 <= ord(c) <= 0x7e:
            raise HandshakeError(
                'Illegal character in subprotocol name: %r' % c)


def parse_host_header(request):
    fields = request.headers_in['Host'].split(':', 1)
    if len(fields) == 1:
        return fields[0], get_default_port(request.is_https())
    try:
        return fields[0], int(fields[1])
    except ValueError, e:
        raise HandshakeError('Invalid port number format: %r' % e)


def format_header(name, value):
    return '%s: %s\r\n' % (name, value)


def build_location(request):
    """Build WebSocket location for request."""
    location_parts = []
    if request.is_https():
        location_parts.append(common.WEB_SOCKET_SECURE_SCHEME)
    else:
        location_parts.append(common.WEB_SOCKET_SCHEME)
    location_parts.append('://')
    host, port = parse_host_header(request)
    connection_port = request.connection.local_addr[1]
    if port != connection_port:
        raise HandshakeError('Header/connection port mismatch: %d/%d' %
                             (port, connection_port))
    location_parts.append(host)
    if (port != get_default_port(request.is_https())):
        location_parts.append(':')
        location_parts.append(str(port))
    location_parts.append(request.uri)
    return ''.join(location_parts)


def get_mandatory_header(request, key):
    value = request.headers_in.get(key)
    if value is None:
        raise HandshakeError('Header %s is not defined' % key)
    return value


def validate_mandatory_header(request, key, expected_value):
    value = get_mandatory_header(request, key)

    if value.lower() != expected_value.lower():
        raise HandshakeError(
            'Expected %r for header %s but found %r (case-insensitive)' %
            (expected_value, key, value))


def check_request_line(request):
    # 5.1 1. The three character UTF-8 string "GET".
    # 5.1 2. A UTF-8-encoded U+0020 SPACE character (0x20 byte).
    if request.method != 'GET':
        raise HandshakeError('Method is not GET')


def check_header_lines(request, mandatory_headers):
    check_request_line(request)

    # The expected field names, and the meaning of their corresponding
    # values, are as follows.
    #  |Upgrade| and |Connection|
    for key, expected_value in mandatory_headers:
        validate_mandatory_header(request, key, expected_value)


def parse_token_list(data):
    """Parses a header value which follows 1#token and returns parsed elements
    as a list of strings.

    Leading LWSes must be trimmed.
    """

    state = http_header_util.ParsingState(data)

    token_list = []

    while True:
        token = http_header_util.consume_token(state)
        if token is not None:
            token_list.append(token)

        http_header_util.consume_lwses(state)

        if http_header_util.peek(state) is None:
            break

        if not http_header_util.consume_string(state, ','):
            raise HandshakeError(
                'Expected a comma but found %r' % http_header_util.peek(state))

        http_header_util.consume_lwses(state)

    if len(token_list) == 0:
        raise HandshakeError('No valid token found')

    return token_list


def _parse_extension_param(state, definition):
    param_name = http_header_util.consume_token(state)

    if param_name is None:
        raise HandshakeError('No valid parameter name found')

    http_header_util.consume_lwses(state)

    if not http_header_util.consume_string(state, '='):
        definition.add_parameter(param_name, None)
        return

    http_header_util.consume_lwses(state)

    param_value = http_header_util.consume_token_or_quoted_string(state)
    if param_value is None:
        raise HandshakeError(
            'No valid parameter value found on the right-hand side of '
            'parameter %r' % param_name)

    definition.add_parameter(param_name, param_value)


def _parse_extension(state):
    extension_token = http_header_util.consume_token(state)
    if extension_token is None:
        return None

    extension = Extension(extension_token)

    while True:
        http_header_util.consume_lwses(state)

        if not http_header_util.consume_string(state, ';'):
            break

        http_header_util.consume_lwses(state)

        try:
            _parse_extension_param(state, extension)
        except HandshakeError, e:
            raise HandshakeError(
                'Failed to parse Sec-WebSocket-Extensions header: '
                'Failed to parse parameter for %r (%r)' %
                (extension_token, e))

    return extension


def parse_extensions(data):
    """Parses Sec-WebSocket-Extensions header value returns a list of
    common.Extension objects.

    Leading LWSes must be trimmed.
    """

    state = http_header_util.ParsingState(data)

    extension_list = []
    while True:
        extension = _parse_extension(state)
        if extension is not None:
            extension_list.append(extension)

        http_header_util.consume_lwses(state)

        if http_header_util.peek(state) is None:
            break

        if not http_header_util.consume_string(state, ','):
            raise HandshakeError(
                'Failed to parse Sec-WebSocket-Extensions header: '
                'Expected a comma but found %r' %
                http_header_util.peek(state))

        http_header_util.consume_lwses(state)

    if len(extension_list) == 0:
        raise HandshakeError(
            'Sec-WebSocket-Extensions header contains no valid extension')

    return extension_list


def format_extensions(extension_list):
    formatted_extension_list = []
    for extension in extension_list:
        formatted_extension_list.append(extension.get_formatted_string())

    return ', '.join(formatted_extension_list)


# vi:sts=4 sw=4 et
