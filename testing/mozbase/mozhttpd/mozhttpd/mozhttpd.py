#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
from SocketServer import ThreadingMixIn

class EasyServer(ThreadingMixIn, BaseHTTPServer.HTTPServer):
    allow_reuse_address = True
    acceptable_errors = (errno.EPIPE, errno.ECONNABORTED)

    def handle_error(self, request, client_address):
        error = sys.exc_value

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

    docroot = os.getcwd() # current working directory at time of import
    proxy_host_dirs = False
    request = None

    def _try_handler(self, method):
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

    def parse_request(self):
        retval = SimpleHTTPServer.SimpleHTTPRequestHandler.parse_request(self)
        self.request = Request(self.path, self.headers, self.rfile)
        return retval

    def do_GET(self):
        if not self._try_handler('GET'):
            if self.docroot:
                # don't include query string and fragment, and prepend
                # host directory if required.
                if self.request.netloc and self.proxy_host_dirs:
                    self.path = '/' + self.request.netloc + \
                        self.request.path
                else:
                    self.path = self.request.path
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
        path = self.docroot
        for word in words:
            drive, word = os.path.splitdrive(word)
            head, word = os.path.split(word)
            if word in (os.curdir, os.pardir): continue
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
    Very basic HTTP server class. Takes a docroot (path on the filesystem)
    and a set of urlhandler dictionaries of the form:

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

    def __init__(self, host="127.0.0.1", port=8888, docroot=None,
                 urlhandlers=None, proxy_host_dirs=False):
        self.host = host
        self.port = int(port)
        self.docroot = docroot
        if not urlhandlers and not docroot:
            self.docroot = os.getcwd()
        self.proxy_host_dirs = proxy_host_dirs
        self.httpd = None
        self.urlhandlers = urlhandlers or []

        class RequestHandlerInstance(RequestHandler):
            docroot = self.docroot
            urlhandlers = self.urlhandlers
            proxy_host_dirs = self.proxy_host_dirs

        self.handler_class = RequestHandlerInstance

    def start(self, block=False):
        """
        Start the server.  If block is True, the call will not return.
        If block is False, the server will be started on a separate thread that
        can be terminated by a call to .stop()
        """
        self.httpd = EasyServer((self.host, self.port), self.handler_class)
        if block:
            self.httpd.serve_forever()
        else:
            self.server = threading.Thread(target=self.httpd.serve_forever)
            self.server.setDaemon(True) # don't hang on exit
            self.server.start()

    def stop(self):
        if self.httpd:
            ### FIXME: There is no shutdown() method in Python 2.4...
            try:
                self.httpd.shutdown()
            except AttributeError:
                pass
        self.httpd = None

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
    parser.add_option('-d', '--docroot', dest='docroot',
                      default=os.getcwd(),
                      help="directory to serve files from [DEFAULT: %default]")
    options, args = parser.parse_args(args)
    if args:
        parser.print_help()
        parser.exit()

    # create the server
    kwargs = options.__dict__.copy()
    server = MozHttpd(**kwargs)

    print "Serving '%s' at %s:%s" % (server.docroot, server.host, server.port)
    server.start(block=True)

if __name__ == '__main__':
    main()
