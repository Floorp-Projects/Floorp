# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import time

from wptserve import server, handlers, routes as default_routes


class FixtureServer(object):

    def __init__(self, root, host="127.0.0.1", port=0):
        if not os.path.isdir(root):
            raise IOError("Server root is not a valid path: {}".format(root))
        self.root = root
        self.host = host
        self.port = port
        self._server = None

    def start(self, block=False):
        if self.alive:
            return
        routes = [("POST", "/file_upload", upload_handler),
                  ("GET", "/slow", slow_loading_document)]
        routes.extend(default_routes.routes)
        self._server = server.WebTestHttpd(
            port=self.port,
            doc_root=self.root,
            routes=routes,
            host=self.host,
        )
        self._server.start(block=block)
        self.port = self._server.httpd.server_port
        self.base_url = self.get_url()

    def stop(self):
        if not self.alive:
            return
        self._server.stop()
        self._server = None

    @property
    def alive(self):
        return self._server is not None

    def get_url(self, path="/"):
        if not self.alive:
            raise Exception("Server not started")
        return self._server.get_url(path)

    @property
    def router(self):
        return self._server.router

    @property
    def routes(self):
        return self._server.router.routes


@handlers.handler
def upload_handler(request, response):
    return 200, [], [request.headers.get("Content-Type")] or []


@handlers.handler
def slow_loading_document(request, response):
    time.sleep(5)
    return """<!doctype html>
<title>ok</title>
<p>ok"""


if __name__ == "__main__":
    here = os.path.abspath(os.path.dirname(__file__))
    doc_root = os.path.join(os.path.dirname(here), "www")
    httpd = FixtureServer(doc_root, port=2829)
    print "Started fixture server on http://{0}:{1}/".format(httpd.host, httpd.port)
    try:
        httpd.start(True)
    except KeyboardInterrupt:
        pass
