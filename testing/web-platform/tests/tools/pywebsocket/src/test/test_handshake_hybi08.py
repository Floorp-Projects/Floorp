#!/usr/bin/env python
#
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


"""Tests for handshake module."""


import unittest

import set_sys_path  # Update sys.path to locate mod_pywebsocket module.
from mod_pywebsocket import common
from mod_pywebsocket.handshake._base import AbortedByUserException
from mod_pywebsocket.handshake._base import HandshakeException
from mod_pywebsocket.handshake._base import VersionException
from mod_pywebsocket.handshake.hybi import Handshaker

from test_handshake_hybi import _create_request
from test_handshake_hybi import _create_handshaker
from test_handshake_hybi import AbortedByUserDispatcher
from test_handshake_hybi import AbortingDispatcher
from test_handshake_hybi import HandshakeAbortedException
from test_handshake_hybi import RequestDefinition
from test_handshake_hybi import SubprotocolChoosingDispatcher
import mock


def _create_good_request_def():
    return RequestDefinition(
        'GET', '/demo',
        {'Host': 'server.example.com',
         'Upgrade': 'websocket',
         'Connection': 'Upgrade',
         'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
         'Sec-WebSocket-Origin': 'http://example.com',
         'Sec-WebSocket-Version': '8'})


_EXPECTED_RESPONSE = (
    'HTTP/1.1 101 Switching Protocols\r\n'
    'Upgrade: websocket\r\n'
    'Connection: Upgrade\r\n'
    'Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n')


class HandshakerTest(unittest.TestCase):
    """A unittest for draft-ietf-hybi-thewebsocketprotocol-06 and later
    handshake processor.
    """

    def test_do_handshake(self):
        request = _create_request(_create_good_request_def())
        dispatcher = mock.MockDispatcher()
        handshaker = Handshaker(request, dispatcher)
        handshaker.do_handshake()

        self.assertTrue(dispatcher.do_extra_handshake_called)

        self.assertEqual(
            _EXPECTED_RESPONSE, request.connection.written_data())
        self.assertEqual('/demo', request.ws_resource)
        self.assertEqual('http://example.com', request.ws_origin)
        self.assertEqual(None, request.ws_protocol)
        self.assertEqual(None, request.ws_extensions)
        self.assertEqual(common.VERSION_HYBI08, request.ws_version)

    def test_do_handshake_with_capitalized_value(self):
        request_def = _create_good_request_def()
        request_def.headers['upgrade'] = 'WEBSOCKET'

        request = _create_request(request_def)
        handshaker = _create_handshaker(request)
        handshaker.do_handshake()
        self.assertEqual(
            _EXPECTED_RESPONSE, request.connection.written_data())

        request_def = _create_good_request_def()
        request_def.headers['Connection'] = 'UPGRADE'

        request = _create_request(request_def)
        handshaker = _create_handshaker(request)
        handshaker.do_handshake()
        self.assertEqual(
            _EXPECTED_RESPONSE, request.connection.written_data())

    def test_do_handshake_with_multiple_connection_values(self):
        request_def = _create_good_request_def()
        request_def.headers['Connection'] = 'Upgrade, keep-alive, , '

        request = _create_request(request_def)
        handshaker = _create_handshaker(request)
        handshaker.do_handshake()
        self.assertEqual(
            _EXPECTED_RESPONSE, request.connection.written_data())

    def test_aborting_handshake(self):
        handshaker = Handshaker(
            _create_request(_create_good_request_def()),
            AbortingDispatcher())
        # do_extra_handshake raises an exception. Check that it's not caught by
        # do_handshake.
        self.assertRaises(HandshakeAbortedException, handshaker.do_handshake)

    def test_do_handshake_with_protocol(self):
        request_def = _create_good_request_def()
        request_def.headers['Sec-WebSocket-Protocol'] = 'chat, superchat'

        request = _create_request(request_def)
        handshaker = Handshaker(request, SubprotocolChoosingDispatcher(0))
        handshaker.do_handshake()

        EXPECTED_RESPONSE = (
            'HTTP/1.1 101 Switching Protocols\r\n'
            'Upgrade: websocket\r\n'
            'Connection: Upgrade\r\n'
            'Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n'
            'Sec-WebSocket-Protocol: chat\r\n\r\n')

        self.assertEqual(EXPECTED_RESPONSE, request.connection.written_data())
        self.assertEqual('chat', request.ws_protocol)

    def test_do_handshake_protocol_not_in_request_but_in_response(self):
        request_def = _create_good_request_def()
        request = _create_request(request_def)
        handshaker = Handshaker(
            request, SubprotocolChoosingDispatcher(-1, 'foobar'))
        # No request has been made but ws_protocol is set. HandshakeException
        # must be raised.
        self.assertRaises(HandshakeException, handshaker.do_handshake)

    def test_do_handshake_with_protocol_no_protocol_selection(self):
        request_def = _create_good_request_def()
        request_def.headers['Sec-WebSocket-Protocol'] = 'chat, superchat'

        request = _create_request(request_def)
        handshaker = _create_handshaker(request)
        # ws_protocol is not set. HandshakeException must be raised.
        self.assertRaises(HandshakeException, handshaker.do_handshake)

    def test_do_handshake_with_extensions(self):
        request_def = _create_good_request_def()
        request_def.headers['Sec-WebSocket-Extensions'] = (
            'permessage-compress; method=deflate, unknown')

        EXPECTED_RESPONSE = (
            'HTTP/1.1 101 Switching Protocols\r\n'
            'Upgrade: websocket\r\n'
            'Connection: Upgrade\r\n'
            'Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n'
            'Sec-WebSocket-Extensions: permessage-compress; method=deflate\r\n'
            '\r\n')

        request = _create_request(request_def)
        handshaker = _create_handshaker(request)
        handshaker.do_handshake()
        self.assertEqual(EXPECTED_RESPONSE, request.connection.written_data())
        self.assertEqual(1, len(request.ws_extensions))
        extension = request.ws_extensions[0]
        self.assertEqual(common.PERMESSAGE_COMPRESSION_EXTENSION,
                         extension.name())
        self.assertEqual(['method'], extension.get_parameter_names())
        self.assertEqual('deflate', extension.get_parameter_value('method'))

    def test_do_handshake_with_quoted_extensions(self):
        request_def = _create_good_request_def()
        request_def.headers['Sec-WebSocket-Extensions'] = (
            'permessage-compress; method=deflate, , '
            'unknown; e   =    "mc^2"; ma="\r\n      \\\rf  "; pv=nrt')

        request = _create_request(request_def)
        handshaker = _create_handshaker(request)
        self.assertRaises(HandshakeException, handshaker.do_handshake)

    def test_do_handshake_with_optional_headers(self):
        request_def = _create_good_request_def()
        request_def.headers['EmptyValue'] = ''
        request_def.headers['AKey'] = 'AValue'

        request = _create_request(request_def)
        handshaker = _create_handshaker(request)
        handshaker.do_handshake()
        self.assertEqual(
            'AValue', request.headers_in['AKey'])
        self.assertEqual(
            '', request.headers_in['EmptyValue'])

    def test_abort_extra_handshake(self):
        handshaker = Handshaker(
            _create_request(_create_good_request_def()),
            AbortedByUserDispatcher())
        # do_extra_handshake raises an AbortedByUserException. Check that it's
        # not caught by do_handshake.
        self.assertRaises(AbortedByUserException, handshaker.do_handshake)

    def test_bad_requests(self):
        bad_cases = [
            ('HTTP request',
             RequestDefinition(
                 'GET', '/demo',
                 {'Host': 'www.google.com',
                  'User-Agent':
                      'Mozilla/5.0 (Macintosh; U; Intel Mac OS X 10.5;'
                      ' en-US; rv:1.9.1.3) Gecko/20090824 Firefox/3.5.3'
                      ' GTB6 GTBA',
                  'Accept':
                      'text/html,application/xhtml+xml,application/xml;q=0.9,'
                      '*/*;q=0.8',
                  'Accept-Language': 'en-us,en;q=0.5',
                  'Accept-Encoding': 'gzip,deflate',
                  'Accept-Charset': 'ISO-8859-1,utf-8;q=0.7,*;q=0.7',
                  'Keep-Alive': '300',
                  'Connection': 'keep-alive'}), None, True)]

        request_def = _create_good_request_def()
        request_def.method = 'POST'
        bad_cases.append(('Wrong method', request_def, None, True))

        request_def = _create_good_request_def()
        del request_def.headers['Host']
        bad_cases.append(('Missing Host', request_def, None, True))

        request_def = _create_good_request_def()
        del request_def.headers['Upgrade']
        bad_cases.append(('Missing Upgrade', request_def, None, True))

        request_def = _create_good_request_def()
        request_def.headers['Upgrade'] = 'nonwebsocket'
        bad_cases.append(('Wrong Upgrade', request_def, None, True))

        request_def = _create_good_request_def()
        del request_def.headers['Connection']
        bad_cases.append(('Missing Connection', request_def, None, True))

        request_def = _create_good_request_def()
        request_def.headers['Connection'] = 'Downgrade'
        bad_cases.append(('Wrong Connection', request_def, None, True))

        request_def = _create_good_request_def()
        del request_def.headers['Sec-WebSocket-Key']
        bad_cases.append(('Missing Sec-WebSocket-Key', request_def, 400, True))

        request_def = _create_good_request_def()
        request_def.headers['Sec-WebSocket-Key'] = (
            'dGhlIHNhbXBsZSBub25jZQ==garbage')
        bad_cases.append(('Wrong Sec-WebSocket-Key (with garbage on the tail)',
                          request_def, 400, True))

        request_def = _create_good_request_def()
        request_def.headers['Sec-WebSocket-Key'] = 'YQ=='  # BASE64 of 'a'
        bad_cases.append(
            ('Wrong Sec-WebSocket-Key (decoded value is not 16 octets long)',
             request_def, 400, True))

        request_def = _create_good_request_def()
        del request_def.headers['Sec-WebSocket-Version']
        bad_cases.append(('Missing Sec-WebSocket-Version', request_def, None,
                          True))

        request_def = _create_good_request_def()
        request_def.headers['Sec-WebSocket-Version'] = '3'
        bad_cases.append(('Wrong Sec-WebSocket-Version', request_def, None,
                          False))

        for (case_name, request_def, expected_status,
             expect_handshake_exception) in bad_cases:
            request = _create_request(request_def)
            handshaker = Handshaker(request, mock.MockDispatcher())
            try:
                handshaker.do_handshake()
                self.fail('No exception thrown for \'%s\' case' % case_name)
            except HandshakeException, e:
                self.assertTrue(expect_handshake_exception)
                self.assertEqual(expected_status, e.status)
            except VersionException, e:
                self.assertFalse(expect_handshake_exception)


if __name__ == '__main__':
    unittest.main()


# vi:sts=4 sw=4 et
