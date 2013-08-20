# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This module contains code for running an HTTP server to view build info.

from __future__ import unicode_literals

import BaseHTTPServer
import json
import os


class HTTPHandler(BaseHTTPServer.BaseHTTPRequestHandler):
    def do_GET(self):
        s = self.server.wrapper
        p = self.path

        if p == '/list':
            self.send_response(200)
            self.send_header('Content-Type', 'application/json; charset=utf-8')
            self.end_headers()

            keys = sorted(s.json_files.keys())
            json.dump({'files': keys}, self.wfile)
            return

        if p.startswith('/resources/'):
            key = p[len('/resources/'):]

            if key not in s.json_files:
                self.send_error(404)
                return

            self.send_response(200)
            self.send_header('Content-Type', 'application/json; charset=utf-8')
            self.end_headers()

            with open(s.json_files[key], 'rb') as fh:
                self.wfile.write(fh.read())

            return

        if p == '/':
            p = '/index.html'

        self.serve_docroot(s.doc_root, p[1:])

    def do_POST(self):
        if self.path == '/shutdown':
            self.server.wrapper.do_shutdown = True
            self.send_response(200)
            return

        self.send_error(404)

    def serve_docroot(self, root, path):
        local_path = os.path.normpath(os.path.join(root, path))

        # Cheap security. This doesn't resolve symlinks, etc. But, it should be
        # acceptable since this server only runs locally.
        if not local_path.startswith(root):
            self.send_error(404)

        if not os.path.exists(local_path):
            self.send_error(404)
            return

        if os.path.isdir(local_path):
            self.send_error(500)
            return

        self.send_response(200)
        ct = 'text/plain'
        if path.endswith('.html'):
            ct = 'text/html'

        self.send_header('Content-Type', ct)
        self.end_headers()

        with open(local_path, 'rb') as fh:
            self.wfile.write(fh.read())


class BuildViewerServer(object):
    def __init__(self, address='localhost', port=0):
        # TODO use pkg_resources to obtain HTML resources.
        pkg_dir = os.path.dirname(os.path.abspath(__file__))
        doc_root = os.path.join(pkg_dir, 'resources', 'html-build-viewer')
        assert os.path.isdir(doc_root)

        self.doc_root = doc_root
        self.json_files = {}

        self.server = BaseHTTPServer.HTTPServer((address, port), HTTPHandler)
        self.server.wrapper = self
        self.do_shutdown = False

    @property
    def url(self):
        hostname, port = self.server.server_address
        return 'http://%s:%d/' % (hostname, port)

    def add_resource_json_file(self, key, path):
        """Register a resource JSON file with the server.

        The file will be made available under the name/key specified."""
        self.json_files[key] = path

    def run(self):
        while not self.do_shutdown:
            self.server.handle_request()
