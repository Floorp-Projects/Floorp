#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""Specialisation of wptserver.server.WebTestHttpd for testing
Marionette.

"""

import argparse
import os
import select
import sys
import time
import urlparse

from wptserve import server, handlers, routes as default_routes


here = os.path.abspath(os.path.dirname(__file__))
default_doc_root = os.path.join(os.path.dirname(here), "www")
default_ssl_cert = os.path.join(here, "test.cert")
default_ssl_key = os.path.join(here, "test.key")


@handlers.handler
def upload_handler(request, response):
    return 200, [], [request.headers.get("Content-Type")] or []


@handlers.handler
def slow_loading_document(request, response):
    time.sleep(5)
    return """<!doctype html>
<title>ok</title>
<p>ok"""


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
                  ("GET", "/slow", slow_loading_document)]
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
    print >>sys.stderr, "%s: started fixture server on %s" % \
        (sys.argv[0], httpd.get_url("/"))
    httpd.wait()
