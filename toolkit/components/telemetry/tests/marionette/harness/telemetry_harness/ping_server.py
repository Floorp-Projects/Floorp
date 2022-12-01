# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import zlib

import mozlog
import wptserve.logger
from marionette_harness.runner import httpd


class PingServer(object):
    """HTTP server for receiving Firefox Client Telemetry pings."""

    def __init__(self, server_root, url):
        self._logger = mozlog.get_default_logger(component="pingserver")

        # Ensure we see logs from wptserve
        try:
            wptserve.logger.set_logger(self._logger)
        except Exception:
            # Raises if already been set
            pass
        self.pings = []

        @httpd.handlers.handler
        def pings_handler(request, response):
            """Handler for HTTP requests to the ping server."""
            request_data = request.body

            if request.headers.get("Content-Encoding") == b"gzip":
                request_data = zlib.decompress(request_data, zlib.MAX_WBITS | 16)

            ping_data = json.loads(request_data)

            # We don't have another channel to hand, so stuff this in the ping payload.
            ping_data["X-PingSender-Version"] = request.headers.get(
                "X-PingSender-Version", b""
            )

            # Store JSON data to self.pings to be used by wait_for_pings()
            self.pings.append(ping_data)

            ping_type = ping_data["type"]

            log_message = "pings_handler received '{}' ping".format(ping_type)

            if ping_type == "main":
                ping_reason = ping_data["payload"]["info"]["reason"]
                log_message = "{} with reason '{}'".format(log_message, ping_reason)

            self._logger.info(log_message)

            status_code = 200
            content = "OK"
            headers = [
                ("Content-Type", "text/plain"),
                ("Content-Length", len(content)),
            ]

            return (status_code, headers, content)

        self._httpd = httpd.FixtureServer(server_root, url=url)
        self._httpd.router.register("POST", "/pings*", pings_handler)

    def get_url(self, *args, **kwargs):
        """Return a URL from the HTTP server."""
        return self._httpd.get_url(*args, **kwargs)

    def start(self):
        """Start the HTTP server."""
        return self._httpd.start()

    def stop(self):
        """Stop the HTTP server."""
        return self._httpd.stop()
