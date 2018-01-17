#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import BaseHTTPServer
import SimpleHTTPServer
import errno
import logging
import threading
import posixpath
import socket
import sys
import os
import urllib
import urlparse
import re
import moznetwork
import time
from SocketServer import ThreadingMixIn


class EasyServer(ThreadingMixIn, BaseHTTPServer.HTTPServer):
    allow_reuse_address = True
    acceptable_errors = (errno.EPIPE, errno.ECONNABORTED)

    def handle_error(self, request, client_address):
        error = sys.exc_info()[1]

        if ((isinstance(error, socket.error) and
             isinstance(error.args, tuple) and
             error.args[0] in self.acceptable_errors)
            or
            (isinstance(error, IOError) and
             error.errno in self.acceptable_errors)):
            pass  # remote hang up before the result is sent
        else:
            logging.error(error)


class Request(object):
    """Details of a request."""

    # attributes from urlsplit that this class also sets
    uri_attrs = ('scheme', 'netloc', 'path', 'query', 'fragment')

    def __init__(self, uri, headers, rfile=None):
        self.uri = uri
        self.headers = headers
        parsed = urlparse.urlsplit(uri)
        for i, attr in enumerate(self.uri_attrs):
            setattr(self, attr, parsed[i])
        try:
            body_len = int(self.headers.get('Content-length', 0))
        except ValueError:
            body_len = 0
        if body_len and rfile:
            self.body = rfile.read(body_len)
        else:
            self.body = None


class RequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):

    docroot = os.getcwd()  # current working directory at time of import
    proxy_host_dirs = False
    request_log = []
    log_requests = False
    request = None

    def __init__(self, *args, **kwargs):
        SimpleHTTPServer.SimpleHTTPRequestHandler.__init__(self, *args, **kwargs)
        self.extensions_map['.svg'] = 'image/svg+xml'

    def _try_handler(self, method):
        if self.log_requests:
            self.request_log.append({'method': method,
                                     'path': self.request.path,
                                     'time': time.time()})

        handlers = [handler for handler in self.urlhandlers
                    if handler['method'] == method]
        for handler in handlers:
            m = re.match(handler['path'], self.request.path)
            if m:
                (response_code, headerdict, data) = \
                    handler['function'](self.request, *m.groups())
                self.send_response(response_code)
                for (keyword, value) in headerdict.iteritems():
                    self.send_header(keyword, value)
                self.end_headers()
                self.wfile.write(data)

                return True

        return False

    def _find_path(self):
        """Find the on-disk path to serve this request from,
        using self.path_mappings and self.docroot.
        Return (url_path, disk_path)."""
        path_components = filter(None, self.request.path.split('/'))
        for prefix, disk_path in self.path_mappings.iteritems():
            prefix_components = filter(None, prefix.split('/'))
            if len(path_components) < len(prefix_components):
                continue
            if path_components[:len(prefix_components)] == prefix_components:
                return ('/'.join(path_components[len(prefix_components):]),
                        disk_path)
        if self.docroot:
            return self.request.path, self.docroot
        return None

    def parse_request(self):
        retval = SimpleHTTPServer.SimpleHTTPRequestHandler.parse_request(self)
        self.request = Request(self.path, self.headers, self.rfile)
        return retval

    def do_GET(self):
        if not self._try_handler('GET'):
            res = self._find_path()
            if res:
                self.path, self.disk_root = res
                # don't include query string and fragment, and prepend
                # host directory if required.
                if self.request.netloc and self.proxy_host_dirs:
                    self.path = '/' + self.request.netloc + \
                        self.path
                SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)
            else:
                self.send_response(404)
                self.end_headers()
                self.wfile.write('')

    def do_POST(self):
        # if we don't have a match, we always fall through to 404 (this may
        # not be "technically" correct if we have a local file at the same
        # path as the resource but... meh)
        if not self._try_handler('POST'):
            self.send_response(404)
            self.end_headers()
            self.wfile.write('')

    def do_DEL(self):
        # if we don't have a match, we always fall through to 404 (this may
        # not be "technically" correct if we have a local file at the same
        # path as the resource but... meh)
        if not self._try_handler('DEL'):
            self.send_response(404)
            self.end_headers()
            self.wfile.write('')

    def translate_path(self, path):
        # this is taken from SimpleHTTPRequestHandler.translate_path(),
        # except we serve from self.docroot instead of os.getcwd(), and
        # parse_request()/do_GET() have already stripped the query string and
        # fragment and mangled the path for proxying, if required.
        path = posixpath.normpath(urllib.unquote(self.path))
        words = path.split('/')
        words = filter(None, words)
        path = self.disk_root
        for word in words:
            drive, word = os.path.splitdrive(word)
            head, word = os.path.split(word)
            if word in (os.curdir, os.pardir):
                continue
            path = os.path.join(path, word)
        return path

    # I found on my local network that calls to this were timing out
    # I believe all of these calls are from log_message
    def address_string(self):
        return "a.b.c.d"

    # This produces a LOT of noise
    def log_message(self, format, *args):
        pass


class MozHttpd(object):
    """
    :param host: Host from which to serve (default 127.0.0.1)
    :param port: Port from which to serve (default 8888)
    :param docroot: Server root (default os.getcwd())
    :param urlhandlers: Handlers to specify behavior against method and path match (default None)
    :param path_mappings: A dict mapping URL prefixes to additional on-disk paths.
    :param proxy_host_dirs: Toggle proxy behavior (default False)
    :param log_requests: Toggle logging behavior (default False)

    Very basic HTTP server class. Takes a docroot (path on the filesystem)
    and a set of urlhandler dictionaries of the form:

    ::

      {
        'method': HTTP method (string): GET, POST, or DEL,
        'path': PATH_INFO (regular expression string),
        'function': function of form fn(arg1, arg2, arg3, ..., request)
      }

    and serves HTTP. For each request, MozHttpd will either return a file
    off the docroot, or dispatch to a handler function (if both path and
    method match).

    Note that one of docroot or urlhandlers may be None (in which case no
    local files or handlers, respectively, will be used). If both docroot or
    urlhandlers are None then MozHttpd will default to serving just the local
    directory.

    MozHttpd also handles proxy requests (i.e. with a full URI on the request
    line).  By default files are served from docroot according to the request
    URI's path component, but if proxy_host_dirs is True, files are served
    from <self.docroot>/<host>/.

    For example, the request "GET http://foo.bar/dir/file.html" would
    (assuming no handlers match) serve <docroot>/dir/file.html if
    proxy_host_dirs is False, or <docroot>/foo.bar/dir/file.html if it is
    True.
    """

    def __init__(self,
                 host="127.0.0.1",
                 port=0,
                 docroot=None,
                 urlhandlers=None,
                 path_mappings=None,
                 proxy_host_dirs=False,
                 log_requests=False):
        self.host = host
        self.port = int(port)
        self.docroot = docroot
        if not (urlhandlers or docroot or path_mappings):
            self.docroot = os.getcwd()
        self.proxy_host_dirs = proxy_host_dirs
        self.httpd = None
        self.urlhandlers = urlhandlers or []
        self.path_mappings = path_mappings or {}
        self.log_requests = log_requests
        self.request_log = []

        class RequestHandlerInstance(RequestHandler):
            docroot = self.docroot
            urlhandlers = self.urlhandlers
            path_mappings = self.path_mappings
            proxy_host_dirs = self.proxy_host_dirs
            request_log = self.request_log
            log_requests = self.log_requests

        self.handler_class = RequestHandlerInstance

    def start(self, block=False):
        """
        Starts the server.

        If `block` is True, the call will not return. If `block` is False, the
        server will be started on a separate thread that can be terminated by
        a call to stop().
        """
        self.httpd = EasyServer((self.host, self.port), self.handler_class)
        if block:
            self.httpd.serve_forever()
        else:
            self.server = threading.Thread(target=self.httpd.serve_forever)
            self.server.setDaemon(True)  # don't hang on exit
            self.server.start()

    def stop(self):
        """
        Stops the server.

        If the server is not running, this method has no effect.
        """
        if self.httpd:
            # FIXME: There is no shutdown() method in Python 2.4...
            try:
                self.httpd.shutdown()
            except AttributeError:
                pass
        self.httpd = None

    def get_url(self, path="/"):
        """
        Returns a URL that can be used for accessing the server (e.g. http://192.168.1.3:4321/)

        :param path: Path to append to URL (e.g. if path were /foobar.html you would get a URL like
                     http://192.168.1.3:4321/foobar.html). Default is `/`.
        """
        if not self.httpd:
            return None

        return "http://%s:%s%s" % (self.host, self.httpd.server_port, path)

    __del__ = stop


def main(args=sys.argv[1:]):

    # parse command line options
    from optparse import OptionParser
    parser = OptionParser()
    parser.add_option('-p', '--port', dest='port',
                      type="int", default=8888,
                      help="port to run the server on [DEFAULT: %default]")
    parser.add_option('-H', '--host', dest='host',
                      default='127.0.0.1',
                      help="host [DEFAULT: %default]")
    parser.add_option('-i', '--external-ip', action="store_true",
                      dest='external_ip', default=False,
                      help="find and use external ip for host")
    parser.add_option('-d', '--docroot', dest='docroot',
                      default=os.getcwd(),
                      help="directory to serve files from [DEFAULT: %default]")
    options, args = parser.parse_args(args)
    if args:
        parser.error("mozhttpd does not take any arguments")

    if options.external_ip:
        host = moznetwork.get_lan_ip()
    else:
        host = options.host

    # create the server
    server = MozHttpd(host=host, port=options.port, docroot=options.docroot)

    print("Serving '%s' at %s:%s" % (server.docroot, server.host, server.port))
    server.start(block=True)


if __name__ == '__main__':
    main()
