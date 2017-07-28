# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/

import requests
import simplejson as json

from telemetry_harness.testcase import TelemetryTestCase


class TestPingServer(TelemetryTestCase):

    def test_ping_server_received_ping(self):

        data = {'type': 'server-test-ping', 'reason': 'unit-test'}
        headers = {'Content-type': 'application/json', 'Accept': 'text/plain'}
        json_req = requests.post(self.ping_server_url, data=json.dumps(data), headers=headers)
        ping = self.wait_for_ping(None, lambda p: p['type'] == 'server-test-ping')
        assert ping is not None
        assert json_req.status_code == 200
        assert data['type'] == ping['type'] and data['reason'] == ping['reason']
