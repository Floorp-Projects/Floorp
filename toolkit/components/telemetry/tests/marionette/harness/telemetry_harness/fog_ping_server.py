# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import zlib

from marionette_harness.runner import httpd
from mozlog import get_default_logger
from six.moves.urllib import parse as urlparse


class FOGPingServer(object):
    """HTTP server for receiving Firefox on Glean pings."""

    def __init__(self, server_root, url):
        self._logger = get_default_logger(component="fog_ping_server")
        self.pings = []

        @httpd.handlers.handler
        def pings_handler(request, response):
            """Handler for HTTP requests to the ping server."""
            request_data = request.body

            if request.headers.get("Content-Encoding") == "gzip":
                request_data = zlib.decompress(request_data, zlib.MAX_WBITS | 16)

            request_url = request.route_match.copy()

            self.pings.append(
                {"request_url": request_url, "payload": json.loads(
                    request_data), "debug_tag": request.headers.get("X-Debug-ID")}
            )

            self._logger.info(
                "pings_handler received '{}' ping".format(request_url["doc_type"])
            )

            status_code = 200
            content = "OK"
            headers = [
                ("Content-Type", "text/plain"),
                ("Content-Length", len(content)),
            ]

            return (status_code, headers, content)

        self._httpd = httpd.FixtureServer(server_root, url=url)

        # See https://mozilla.github.io/glean/book/user/pings/index.html#ping-submission
        self._httpd.router.register(
            "POST",
            "/submit/{application_id}/{doc_type}/{glean_schema_version}/{document_id}",
            pings_handler,
        )

    @property
    def url(self):
        """Return the URL for the running HTTP FixtureServer."""
        return self._httpd.get_url("/")

    @property
    def port(self):
        """Return the port for the running HTTP FixtureServer."""
        parse_result = urlparse.urlparse(self.url)
        return parse_result.port

    def start(self):
        """Start the HTTP FixtureServer."""
        return self._httpd.start()

    def stop(self):
        """Stop the HTTP FixtureServer."""
        return self._httpd.stop()
