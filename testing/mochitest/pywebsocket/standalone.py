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


"""Standalone WebSocket server.

Use this server to run mod_pywebsocket without Apache HTTP Server.

Usage:
    python standalone.py [-p <ws_port>] [-w <websock_handlers>]
                         [-s <scan_dir>]
                         [-d <document_root>]
                         [-m <websock_handlers_map_file>]
                         ... for other options, see _main below ...

<ws_port> is the port number to use for ws:// connection.

<document_root> is the path to the root directory of HTML files.

<websock_handlers> is the path to the root directory of WebSocket handlers.
See __init__.py for details of <websock_handlers> and how to write WebSocket
handlers. If this path is relative, <document_root> is used as the base.

<scan_dir> is a path under the root directory. If specified, only the
handlers under scan_dir are scanned. This is useful in saving scan time.

Note:
This server is derived from SocketServer.ThreadingMixIn. Hence a thread is
used for each request.

SECURITY WARNING: This uses CGIHTTPServer and CGIHTTPServer is not secure.
It may execute arbitrary Python code or external programs. It should not be
used outside a firewall.
"""

import BaseHTTPServer
import CGIHTTPServer
import SimpleHTTPServer
import SocketServer
import logging
import logging.handlers
import optparse
import os
import re
import select
import socket
import sys
import threading

_HAS_OPEN_SSL = False
try:
    import OpenSSL.SSL
    _HAS_OPEN_SSL = True
except ImportError:
    pass

from mod_pywebsocket import common
from mod_pywebsocket import dispatch
from mod_pywebsocket import handshake
from mod_pywebsocket import http_header_util
from mod_pywebsocket import memorizingfile
from mod_pywebsocket import util


_DEFAULT_LOG_MAX_BYTES = 1024 * 256
_DEFAULT_LOG_BACKUP_COUNT = 5

_DEFAULT_REQUEST_QUEUE_SIZE = 128

# 1024 is practically large enough to contain WebSocket handshake lines.
_MAX_MEMORIZED_LINES = 1024


def _print_warnings_if_any(dispatcher):
    warnings = dispatcher.source_warnings()
    if warnings:
        for warning in warnings:
            logging.warning('mod_pywebsocket: %s' % warning)


class _StandaloneConnection(object):
    """Mimic mod_python mp_conn."""

    def __init__(self, request_handler):
        """Construct an instance.

        Args:
            request_handler: A WebSocketRequestHandler instance.
        """

        self._request_handler = request_handler

    def get_local_addr(self):
        """Getter to mimic mp_conn.local_addr."""

        return (self._request_handler.server.server_name,
                self._request_handler.server.server_port)
    local_addr = property(get_local_addr)

    def get_remote_addr(self):
        """Getter to mimic mp_conn.remote_addr.

        Setting the property in __init__ won't work because the request
        handler is not initialized yet there."""

        return self._request_handler.client_address
    remote_addr = property(get_remote_addr)

    def write(self, data):
        """Mimic mp_conn.write()."""

        return self._request_handler.wfile.write(data)

    def read(self, length):
        """Mimic mp_conn.read()."""

        return self._request_handler.rfile.read(length)

    def get_memorized_lines(self):
        """Get memorized lines."""

        return self._request_handler.rfile.get_memorized_lines()


class _StandaloneRequest(object):
    """Mimic mod_python request."""

    def __init__(self, request_handler, use_tls):
        """Construct an instance.

        Args:
            request_handler: A WebSocketRequestHandler instance.
        """

        self._logger = util.get_class_logger(self)

        self._request_handler = request_handler
        self.connection = _StandaloneConnection(request_handler)
        self._use_tls = use_tls

    def get_uri(self):
        """Getter to mimic request.uri."""

        return self._request_handler.path
    uri = property(get_uri)

    def get_method(self):
        """Getter to mimic request.method."""

        return self._request_handler.command
    method = property(get_method)

    def get_headers_in(self):
        """Getter to mimic request.headers_in."""

        return self._request_handler.headers
    headers_in = property(get_headers_in)

    def is_https(self):
        """Mimic request.is_https()."""

        return self._use_tls

    def _drain_received_data(self):
        """Don't use this method from WebSocket handler. Drains unread data
        in the receive buffer.
        """

        raw_socket = self._request_handler.connection
        drained_data = util.drain_received_data(raw_socket)

        if drained_data:
            self._logger.debug(
                'Drained data following close frame: %r', drained_data)


class WebSocketServer(SocketServer.ThreadingMixIn, BaseHTTPServer.HTTPServer):
    """HTTPServer specialized for WebSocket."""

    # Overrides SocketServer.ThreadingMixIn.daemon_threads
    daemon_threads = True
    # Overrides BaseHTTPServer.HTTPServer.allow_reuse_address
    allow_reuse_address = True

    def __init__(self, options):
        """Override SocketServer.TCPServer.__init__ to set SSL enabled
        socket object to self.socket before server_bind and server_activate,
        if necessary.
        """

        self.request_queue_size = options.request_queue_size
        self.__ws_is_shut_down = threading.Event()
        self.__ws_serving = False

        SocketServer.BaseServer.__init__(
            self, (options.server_host, options.port), WebSocketRequestHandler)

        # Expose the options object to allow handler objects access it. We name
        # it with websocket_ prefix to avoid conflict.
        self.websocket_server_options = options

        self._create_sockets()
        self.server_bind()
        self.server_activate()

    def _create_sockets(self):
        self.server_name, self.server_port = self.server_address
        self._sockets = []
        if not self.server_name:
            addrinfo_array = [
                (self.address_family, self.socket_type, '', '', '')]
        else:
            addrinfo_array = socket.getaddrinfo(self.server_name,
                                                self.server_port,
                                                socket.AF_UNSPEC,
                                                socket.SOCK_STREAM,
                                                socket.IPPROTO_TCP)
        for addrinfo in addrinfo_array:
            logging.info('Create socket on: %r', addrinfo)
            family, socktype, proto, canonname, sockaddr = addrinfo
            try:
                socket_ = socket.socket(family, socktype)
            except Exception, e:
                logging.info('Skip by failure: %r', e)
                continue
            if self.websocket_server_options.use_tls:
                ctx = OpenSSL.SSL.Context(OpenSSL.SSL.SSLv23_METHOD)
                ctx.use_privatekey_file(
                    self.websocket_server_options.private_key)
                ctx.use_certificate_file(
                    self.websocket_server_options.certificate)
                socket_ = OpenSSL.SSL.Connection(ctx, socket_)
            self._sockets.append((socket_, addrinfo))

    def server_bind(self):
        """Override SocketServer.TCPServer.server_bind to enable multiple
        sockets bind.
        """

        for socket_, addrinfo in self._sockets:
            logging.info('Bind on: %r', addrinfo)
            if self.allow_reuse_address:
                socket_.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            socket_.bind(self.server_address)

    def server_activate(self):
        """Override SocketServer.TCPServer.server_activate to enable multiple
        sockets listen.
        """

        failed_sockets = []

        for socketinfo in self._sockets:
            socket_, addrinfo = socketinfo
            logging.info('Listen on: %r', addrinfo)
            try:
                socket_.listen(self.request_queue_size)
            except Exception, e:
                logging.info('Skip by failure: %r', e)
                socket_.close()
                failed_sockets.append(socketinfo)

        for socketinfo in failed_sockets:
            self._sockets.remove(socketinfo)

    def server_close(self):
        """Override SocketServer.TCPServer.server_close to enable multiple
        sockets close.
        """

        for socketinfo in self._sockets:
            socket_, addrinfo = socketinfo
            logging.info('Close on: %r', addrinfo)
            socket_.close()

    def fileno(self):
        """Override SocketServer.TCPServer.fileno."""

        logging.critical('Not supported: fileno')
        return self._sockets[0][0].fileno()

    def handle_error(self, rquest, client_address):
        """Override SocketServer.handle_error."""

        logging.error(
            ('Exception in processing request from: %r' % (client_address,)) +
            '\n' + util.get_stack_trace())
        # Note: client_address is a tuple. To match it against %r, we need the
        # trailing comma.

    def serve_forever(self, poll_interval=0.5):
        """Override SocketServer.BaseServer.serve_forever."""

        self.__ws_serving = True
        self.__ws_is_shut_down.clear()
        handle_request = self.handle_request
        if hasattr(self, '_handle_request_noblock'):
            handle_request = self._handle_request_noblock
        else:
            logging.warning('mod_pywebsocket: fallback to blocking request '
                            'handler')
        try:
            while self.__ws_serving:
                r, w, e = select.select(
                    [socket_[0] for socket_ in self._sockets],
                    [], [], poll_interval)
                for socket_ in r:
                    self.socket = socket_
                    handle_request()
                self.socket = None
        finally:
            self.__ws_is_shut_down.set()

    def shutdown(self):
        """Override SocketServer.BaseServer.shutdown."""

        self.__ws_serving = False
        self.__ws_is_shut_down.wait()


class WebSocketRequestHandler(CGIHTTPServer.CGIHTTPRequestHandler):
    """CGIHTTPRequestHandler specialized for WebSocket."""

    def setup(self):
        """Override SocketServer.StreamRequestHandler.setup to wrap rfile
        with MemorizingFile.

        This method will be called by BaseRequestHandler's constructor
        before calling BaseHTTPRequestHandler.handle.
        BaseHTTPRequestHandler.handle will call
        BaseHTTPRequestHandler.handle_one_request and it will call
        WebSocketRequestHandler.parse_request.
        """

        # Call superclass's setup to prepare rfile, wfile, etc. See setup
        # definition on the root class SocketServer.StreamRequestHandler to
        # understand what this does.
        CGIHTTPServer.CGIHTTPRequestHandler.setup(self)

        self.rfile = memorizingfile.MemorizingFile(
            self.rfile,
            max_memorized_lines=_MAX_MEMORIZED_LINES)

    def __init__(self, request, client_address, server):
        self._options = server.websocket_server_options

        # Overrides CGIHTTPServerRequestHandler.cgi_directories.
        self.cgi_directories = self._options.cgi_directories
        # Replace CGIHTTPRequestHandler.is_executable method.
        if self._options.is_executable_method is not None:
            self.is_executable = self._options.is_executable_method

        self._request = _StandaloneRequest(self, self._options.use_tls)

        _print_warnings_if_any(self._options.dispatcher)

        # This actually calls BaseRequestHandler.__init__.
        CGIHTTPServer.CGIHTTPRequestHandler.__init__(
            self, request, client_address, server)

    def parse_request(self):
        """Override BaseHTTPServer.BaseHTTPRequestHandler.parse_request.

        Return True to continue processing for HTTP(S), False otherwise.

        See BaseHTTPRequestHandler.handle_one_request method which calls
        this method to understand how the return value will be handled.
        """

        # We hook parse_request method, but also call the original
        # CGIHTTPRequestHandler.parse_request since when we return False,
        # CGIHTTPRequestHandler.handle_one_request continues processing and
        # it needs variables set by CGIHTTPRequestHandler.parse_request.
        #
        # Variables set by this method will be also used by WebSocket request
        # handling. See _StandaloneRequest.get_request, etc.
        if not CGIHTTPServer.CGIHTTPRequestHandler.parse_request(self):
            return False
        host, port, resource = http_header_util.parse_uri(self.path)
        if resource is None:
            logging.info('mod_pywebsocket: invalid uri %r' % self.path)
            return True
        server_options = self.server.websocket_server_options
        if host is not None:
            validation_host = server_options.validation_host
            if validation_host is not None and host != validation_host:
                logging.info('mod_pywebsocket: invalid host %r '
                             '(expected: %r)' % (host, validation_host))
                return True
        if port is not None:
            validation_port = server_options.validation_port
            if validation_port is not None and port != validation_port:
                logging.info('mod_pywebsocket: invalid port %r '
                             '(expected: %r)' % (port, validation_port))
                return True
        self.path = resource

        try:
            # Fallback to default http handler for request paths for which
            # we don't have request handlers.
            if not self._options.dispatcher.get_handler_suite(self.path):
                logging.info('No handlers for request: %s' % self.path)
                return True

            try:
                handshake.do_handshake(
                    self._request,
                    self._options.dispatcher,
                    allowDraft75=self._options.allow_draft75,
                    strict=self._options.strict)
            except handshake.AbortedByUserException, e:
                logging.info('mod_pywebsocket: %s' % e)
                return False
            try:
                self._request._dispatcher = self._options.dispatcher
                self._options.dispatcher.transfer_data(self._request)
            except dispatch.DispatchException, e:
                logging.warning('mod_pywebsocket: %s' % e)
                return False
            except handshake.AbortedByUserException, e:
                logging.info('mod_pywebsocket: %s' % e)
            except Exception, e:
                # Catch exception in transfer_data.
                # In this case, handshake has been successful, so just log
                # the exception and return False.
                logging.info('mod_pywebsocket: %s' % e)
                logging.info(
                    'mod_pywebsocket: %s' % util.get_stack_trace())
        except dispatch.DispatchException, e:
            logging.warning('mod_pywebsocket: %s' % e)
            self.send_error(e.status)
        except handshake.HandshakeException, e:
            # Handshake for ws(s) failed. Assume http(s).
            logging.info('mod_pywebsocket: %s' % e)
            self.send_error(e.status)
        except Exception, e:
            logging.warning('mod_pywebsocket: %s' % e)
            logging.warning('mod_pywebsocket: %s' % util.get_stack_trace())
        return False

    def log_request(self, code='-', size='-'):
        """Override BaseHTTPServer.log_request."""

        logging.info('"%s" %s %s',
                     self.requestline, str(code), str(size))

    def log_error(self, *args):
        """Override BaseHTTPServer.log_error."""

        # Despite the name, this method is for warnings than for errors.
        # For example, HTTP status code is logged by this method.
        logging.warning('%s - %s' %
                        (self.address_string(), (args[0] % args[1:])))

    def is_cgi(self):
        """Test whether self.path corresponds to a CGI script.

        Add extra check that self.path doesn't contains ..
        Also check if the file is a executable file or not.
        If the file is not executable, it is handled as static file or dir
        rather than a CGI script.
        """

        if CGIHTTPServer.CGIHTTPRequestHandler.is_cgi(self):
            if '..' in self.path:
                return False
            # strip query parameter from request path
            resource_name = self.path.split('?', 2)[0]
            # convert resource_name into real path name in filesystem.
            scriptfile = self.translate_path(resource_name)
            if not os.path.isfile(scriptfile):
                return False
            if not self.is_executable(scriptfile):
                return False
            return True
        return False


def _configure_logging(options):
    logger = logging.getLogger()
    logger.setLevel(logging.getLevelName(options.log_level.upper()))
    if options.log_file:
        handler = logging.handlers.RotatingFileHandler(
                options.log_file, 'a', options.log_max, options.log_count)
    else:
        handler = logging.StreamHandler()
    formatter = logging.Formatter(
            '[%(asctime)s] [%(levelname)s] %(name)s: %(message)s')
    handler.setFormatter(formatter)
    logger.addHandler(handler)


def _alias_handlers(dispatcher, websock_handlers_map_file):
    """Set aliases specified in websock_handler_map_file in dispatcher.

    Args:
        dispatcher: dispatch.Dispatcher instance
        websock_handler_map_file: alias map file
    """

    fp = open(websock_handlers_map_file)
    try:
        for line in fp:
            if line[0] == '#' or line.isspace():
                continue
            m = re.match('(\S+)\s+(\S+)', line)
            if not m:
                logging.warning('Wrong format in map file:' + line)
                continue
            try:
                dispatcher.add_resource_path_alias(
                    m.group(1), m.group(2))
            except dispatch.DispatchException, e:
                logging.error(str(e))
    finally:
        fp.close()


def _main():
    parser = optparse.OptionParser()
    parser.add_option('-H', '--server-host', '--server_host',
                      dest='server_host',
                      default='',
                      help='server hostname to listen to')
    parser.add_option('-V', '--validation-host', '--validation_host',
                      dest='validation_host',
                      default=None,
                      help='server hostname to validate in absolute path.')
    parser.add_option('-p', '--port', dest='port', type='int',
                      default=common.DEFAULT_WEB_SOCKET_PORT,
                      help='port to listen to')
    parser.add_option('-P', '--validation-port', '--validation_port',
                      dest='validation_port', type='int',
                      default=None,
                      help='server port to validate in absolute path.')
    parser.add_option('-w', '--websock-handlers', '--websock_handlers',
                      dest='websock_handlers',
                      default='.',
                      help='WebSocket handlers root directory.')
    parser.add_option('-m', '--websock-handlers-map-file',
                      '--websock_handlers_map_file',
                      dest='websock_handlers_map_file',
                      default=None,
                      help=('WebSocket handlers map file. '
                            'Each line consists of alias_resource_path and '
                            'existing_resource_path, separated by spaces.'))
    parser.add_option('-s', '--scan-dir', '--scan_dir', dest='scan_dir',
                      default=None,
                      help=('WebSocket handlers scan directory. '
                            'Must be a directory under websock_handlers.'))
    parser.add_option('-d', '--document-root', '--document_root',
                      dest='document_root', default='.',
                      help='Document root directory.')
    parser.add_option('-x', '--cgi-paths', '--cgi_paths', dest='cgi_paths',
                      default=None,
                      help=('CGI paths relative to document_root.'
                            'Comma-separated. (e.g -x /cgi,/htbin) '
                            'Files under document_root/cgi_path are handled '
                            'as CGI programs. Must be executable.'))
    parser.add_option('-t', '--tls', dest='use_tls', action='store_true',
                      default=False, help='use TLS (wss://)')
    parser.add_option('-k', '--private-key', '--private_key',
                      dest='private_key',
                      default='', help='TLS private key file.')
    parser.add_option('-c', '--certificate', dest='certificate',
                      default='', help='TLS certificate file.')
    parser.add_option('-l', '--log-file', '--log_file', dest='log_file',
                      default='', help='Log file.')
    parser.add_option('--log-level', '--log_level', type='choice',
                      dest='log_level', default='warn',
                      choices=['debug', 'info', 'warning', 'warn', 'error',
                               'critical'],
                      help='Log level.')
    parser.add_option('--log-max', '--log_max', dest='log_max', type='int',
                      default=_DEFAULT_LOG_MAX_BYTES,
                      help='Log maximum bytes')
    parser.add_option('--log-count', '--log_count', dest='log_count',
                      type='int', default=_DEFAULT_LOG_BACKUP_COUNT,
                      help='Log backup count')
    parser.add_option('--allow-draft75', dest='allow_draft75',
                      action='store_true', default=False,
                      help='Allow draft 75 handshake')
    parser.add_option('--strict', dest='strict', action='store_true',
                      default=False, help='Strictly check handshake request')
    parser.add_option('-q', '--queue', dest='request_queue_size', type='int',
                      default=_DEFAULT_REQUEST_QUEUE_SIZE,
                      help='request queue size')
    options = parser.parse_args()[0]

    os.chdir(options.document_root)

    _configure_logging(options)

    # TODO(tyoshino): Clean up initialization of CGI related values. Move some
    # of code here to WebSocketRequestHandler class if it's better.
    options.cgi_directories = []
    options.is_executable_method = None
    if options.cgi_paths:
        options.cgi_directories = options.cgi_paths.split(',')
        if sys.platform in ('cygwin', 'win32'):
            cygwin_path = None
            # For Win32 Python, it is expected that CYGWIN_PATH
            # is set to a directory of cygwin binaries.
            # For example, websocket_server.py in Chromium sets CYGWIN_PATH to
            # full path of third_party/cygwin/bin.
            if 'CYGWIN_PATH' in os.environ:
                cygwin_path = os.environ['CYGWIN_PATH']
            util.wrap_popen3_for_win(cygwin_path)

            def __check_script(scriptpath):
                return util.get_script_interp(scriptpath, cygwin_path)

            options.is_executable_method = __check_script

    if options.use_tls:
        if not _HAS_OPEN_SSL:
            logging.critical('To use TLS, install pyOpenSSL.')
            sys.exit(1)
        if not options.private_key or not options.certificate:
            logging.critical(
                    'To use TLS, specify private_key and certificate.')
            sys.exit(1)

    if not options.scan_dir:
        options.scan_dir = options.websock_handlers

    try:
        # Share a Dispatcher among request handlers to save time for
        # instantiation.  Dispatcher can be shared because it is thread-safe.
        options.dispatcher = dispatch.Dispatcher(options.websock_handlers,
                                                 options.scan_dir)
        if options.websock_handlers_map_file:
            _alias_handlers(options.dispatcher,
                            options.websock_handlers_map_file)
        _print_warnings_if_any(options.dispatcher)

        server = WebSocketServer(options)
        server.serve_forever()
    except Exception, e:
        logging.critical('mod_pywebsocket: %s' % e)
        logging.critical('mod_pywebsocket: %s' % util.get_stack_trace())
        sys.exit(1)


if __name__ == '__main__':
    _main()


# vi:sts=4 sw=4 et
