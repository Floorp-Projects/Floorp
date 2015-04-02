# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

from mozhttpd import MozHttpd


class FixtureServer(object):

    def __init__(self, root, host="127.0.0.1", port=0):
        if not os.path.isdir(root):
            raise Exception("Server root is not a valid path: %s" % root)
        self.root = root
        self.host = host
        self.port = port
        self._server = None

    def start(self, block=False):
        if self.alive:
            return
        self._server = MozHttpd(host=self.host, port=self.port, docroot=self.root, urlhandlers=[
                                {"method": "POST", "path": "/file_upload", "function": upload_handler}])
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
            raise "Server not started"
        return self._server.get_url(path)

    @property
    def urlhandlers(self):
        return self._server.urlhandlers


def upload_handler(query, postdata=None):
    return (200, {}, query.headers.getheader("Content-Type"))


if __name__ == "__main__":
    here = os.path.abspath(os.path.dirname(__file__))
    root = os.path.join(os.path.dirname(here), "www")
    httpd = FixtureServer(root, port=2829)
    print "Started fixture server on http://%s:%d/" % (httpd.host, httpd.port)
    try:
        httpd.start(True)
    except KeyboardInterrupt:
        pass
