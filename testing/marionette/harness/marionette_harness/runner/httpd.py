#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""Specialisation of wptserver.server.WebTestHttpd for testing
Marionette.

"""

from __future__ import absolute_import, print_function

import argparse
import os
import select
import sys
import time
import urlparse

from wptserve import (
    handlers,
    request,
    routes as default_routes,
    server
)


root = os.path.abspath(os.path.dirname(os.path.dirname(__file__)))
default_doc_root = os.path.join(root, "www")
default_ssl_cert = os.path.join(root, "certificates", "test.cert")
default_ssl_key = os.path.join(root, "certificates", "test.key")


@handlers.handler
def http_auth_handler(req, response):
    # Allow the test to specify the username and password
    params = dict(urlparse.parse_qsl(req.url_parts.query))
    username = params.get("username", "guest")
    password = params.get("password", "guest")

    auth = request.Authentication(req.headers)
    content = """<!doctype html>
<title>HTTP Authentication</title>
<p id="status">{}</p>"""

    if auth.username == username and auth.password == password:
        response.status = 200
        response.content = content.format("success")

    else:
        response.status = 401
        response.headers.set("WWW-Authenticate", "Basic realm=\"secret\"")
        response.content = content.format("restricted")


@handlers.handler
def upload_handler(request, response):
    return 200, [], [request.headers.get("Content-Type")] or []


@handlers.handler
def slow_loading_handler(request, response):
    # Allow the test specify the delay for delivering the content
    params = dict(urlparse.parse_qsl(request.url_parts.query))
    delay = int(params.get('delay', 5))
    time.sleep(delay)

    # Do not allow the page to be cached to circumvent the bfcache of the browser
    response.headers.set("Cache-Control", "no-cache, no-store")
    response.content = """<!doctype html>
<meta charset="UTF-8">
<title>Slow page loading</title>

<p>Delay: <span id="delay">{}</span></p>
""".format(delay)


class NotAliveError(Exception):
    """Occurs when attempting to run a function that requires the HTTPD
    to have been started, and it has not.

    """
    pass


class FixtureServer(object):

    def __init__(self, doc_root, url="http://127.0.0.1:0", use_ssl=False,
                 ssl_cert=None, ssl_key=None):
        if not os.path.isdir(doc_root):
            raise ValueError("Server root is not a directory: %s" % doc_root)

        url = urlparse.urlparse(url)
        if url.scheme is None:
            raise ValueError("Server scheme not provided")

        scheme, host, port = url.scheme, url.hostname, url.port
        if host is None:
            host = "127.0.0.1"
        if port is None:
            port = 0

        routes = [("POST", "/file_upload", upload_handler),
                  ("GET", "/http_auth", http_auth_handler),
                  ("GET", "/slow", slow_loading_handler),
                  ]
        routes.extend(default_routes.routes)

        self._httpd = server.WebTestHttpd(host=host,
                                          port=port,
                                          bind_hostname=True,
                                          doc_root=doc_root,
                                          routes=routes,
                                          use_ssl=True if scheme == "https" else False,
                                          certificate=ssl_cert,
                                          key_file=ssl_key)

    def start(self, block=False):
        if self.is_alive:
            return
        self._httpd.start(block=block)

    def wait(self):
        if not self.is_alive:
            return
        try:
            select.select([], [], [])
        except KeyboardInterrupt:
            self.stop()

    def stop(self):
        if not self.is_alive:
            return
        self._httpd.stop()

    def get_url(self, path):
        if not self.is_alive:
            raise NotAliveError()
        return self._httpd.get_url(path)

    @property
    def doc_root(self):
        return self._httpd.router.doc_root

    @property
    def router(self):
        return self._httpd.router

    @property
    def routes(self):
        return self._httpd.router.routes

    @property
    def is_alive(self):
        return self._httpd.started


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Specialised HTTP server for testing Marionette.")
    parser.add_argument("url", help="""
service address including scheme, hostname, port, and prefix for document root,
e.g. \"https://0.0.0.0:0/base/\"""")
    parser.add_argument(
        "-r", dest="doc_root", default=default_doc_root,
        help="path to document root (default %(default)s)")
    parser.add_argument(
        "-c", dest="ssl_cert", default=default_ssl_cert,
        help="path to SSL certificate (default %(default)s)")
    parser.add_argument(
        "-k", dest="ssl_key", default=default_ssl_key,
        help="path to SSL certificate key (default %(default)s)")
    args = parser.parse_args()

    httpd = FixtureServer(args.doc_root, args.url,
                          ssl_cert=args.ssl_cert,
                          ssl_key=args.ssl_key)
    httpd.start()
    print("{0}: started fixture server on {1}".format(sys.argv[0], httpd.get_url("/")),
          file=sys.stderr)
    httpd.wait()
