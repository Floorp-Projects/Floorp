#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""Tests for requests_unixsocket"""

import logging

import pytest
import requests

import requests_unixsocket
from requests_unixsocket.testutils import UnixSocketServerThread


logger = logging.getLogger(__name__)


def test_unix_domain_adapter_ok():
    with UnixSocketServerThread() as usock_thread:
        session = requests_unixsocket.Session('http+unix://')
        urlencoded_usock = requests.compat.quote_plus(usock_thread.usock)
        url = 'http+unix://%s/path/to/page' % urlencoded_usock

        for method in ['get', 'post', 'head', 'patch', 'put', 'delete',
                       'options']:
            logger.debug('Calling session.%s(%r) ...', method, url)
            r = getattr(session, method)(url)
            logger.debug(
                'Received response: %r with text: %r and headers: %r',
                r, r.text, r.headers)
            assert r.status_code == 200
            assert r.headers['server'] == 'waitress'
            assert r.headers['X-Transport'] == 'unix domain socket'
            assert r.headers['X-Requested-Path'] == '/path/to/page'
            assert r.headers['X-Socket-Path'] == usock_thread.usock
            assert isinstance(r.connection, requests_unixsocket.UnixAdapter)
            assert r.url == url
            if method == 'head':
                assert r.text == ''
            else:
                assert r.text == 'Hello world!'


def test_unix_domain_adapter_url_with_query_params():
    with UnixSocketServerThread() as usock_thread:
        session = requests_unixsocket.Session('http+unix://')
        urlencoded_usock = requests.compat.quote_plus(usock_thread.usock)
        url = ('http+unix://%s'
               '/containers/nginx/logs?timestamp=true' % urlencoded_usock)

        for method in ['get', 'post', 'head', 'patch', 'put', 'delete',
                       'options']:
            logger.debug('Calling session.%s(%r) ...', method, url)
            r = getattr(session, method)(url)
            logger.debug(
                'Received response: %r with text: %r and headers: %r',
                r, r.text, r.headers)
            assert r.status_code == 200
            assert r.headers['server'] == 'waitress'
            assert r.headers['X-Transport'] == 'unix domain socket'
            assert r.headers['X-Requested-Path'] == '/containers/nginx/logs'
            assert r.headers['X-Requested-Query-String'] == 'timestamp=true'
            assert r.headers['X-Socket-Path'] == usock_thread.usock
            assert isinstance(r.connection, requests_unixsocket.UnixAdapter)
            assert r.url == url
            if method == 'head':
                assert r.text == ''
            else:
                assert r.text == 'Hello world!'


def test_unix_domain_adapter_connection_error():
    session = requests_unixsocket.Session('http+unix://')

    for method in ['get', 'post', 'head', 'patch', 'put', 'delete', 'options']:
        with pytest.raises(requests.ConnectionError):
            getattr(session, method)(
                'http+unix://socket_does_not_exist/path/to/page')


def test_unix_domain_adapter_connection_proxies_error():
    session = requests_unixsocket.Session('http+unix://')

    for method in ['get', 'post', 'head', 'patch', 'put', 'delete', 'options']:
        with pytest.raises(ValueError) as excinfo:
            getattr(session, method)(
                'http+unix://socket_does_not_exist/path/to/page',
                proxies={"http+unix": "http://10.10.1.10:1080"})
        assert ('UnixAdapter does not support specifying proxies'
                in str(excinfo.value))


def test_unix_domain_adapter_monkeypatch():
    with UnixSocketServerThread() as usock_thread:
        with requests_unixsocket.monkeypatch('http+unix://'):
            urlencoded_usock = requests.compat.quote_plus(usock_thread.usock)
            url = 'http+unix://%s/path/to/page' % urlencoded_usock

            for method in ['get', 'post', 'head', 'patch', 'put', 'delete',
                           'options']:
                logger.debug('Calling session.%s(%r) ...', method, url)
                r = getattr(requests, method)(url)
                logger.debug(
                    'Received response: %r with text: %r and headers: %r',
                    r, r.text, r.headers)
                assert r.status_code == 200
                assert r.headers['server'] == 'waitress'
                assert r.headers['X-Transport'] == 'unix domain socket'
                assert r.headers['X-Requested-Path'] == '/path/to/page'
                assert r.headers['X-Socket-Path'] == usock_thread.usock
                assert isinstance(r.connection,
                                  requests_unixsocket.UnixAdapter)
                assert r.url == url
                if method == 'head':
                    assert r.text == ''
                else:
                    assert r.text == 'Hello world!'

    for method in ['get', 'post', 'head', 'patch', 'put', 'delete', 'options']:
        with pytest.raises(requests.exceptions.InvalidSchema):
            getattr(requests, method)(url)
