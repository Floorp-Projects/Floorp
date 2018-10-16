# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/

import requests

from telemetry_harness.testcase import TelemetryTestCase


class TestPingServer(TelemetryTestCase):
    def test_ping_server_received_ping(self):
        ping_type = "server-test-ping"
        ping_reason = "unit-test"

        def action_func():
            """Perform a POST request to the ping server."""
            data = {"type": ping_type, "reason": ping_reason}
            headers = {"Content-type": "application/json", "Accept": "text/plain"}

            response = requests.post(self.ping_server_url, json=data, headers=headers)

            self.assertEqual(
                response.status_code,
                200,
                msg="Error sending POST request to ping server: {response.text}".format(
                    response=response
                ),
            )
            return response

        def ping_filter_func(ping):
            return ping["type"] == ping_type

        ping = self.wait_for_ping(action_func, ping_filter_func)

        self.assertIsNotNone(ping)
        self.assertEqual(ping["type"], ping_type)
        self.assertEqual(ping["reason"], ping_reason)
