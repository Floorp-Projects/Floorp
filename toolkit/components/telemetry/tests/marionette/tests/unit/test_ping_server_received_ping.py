# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/

import requests
import simplejson as json

from telemetry_harness.testcase import TelemetryTestCase


class TestPingServer(TelemetryTestCase):

    def test_ping_server_received_ping(self):

        data = {'sender': 'John', 'receiver': 'Joe', 'message': 'We did it!'}
        headers = {'Content-type': 'application/json', 'Accept': 'text/plain'}
        json_req = requests.post(self.ping_server_url, data=json.dumps(data), headers=headers)
        assert json_req.status_code == 200
        assert len(self.ping_list) == 1
        assert data['sender'] == self.ping_list[-1]['sender']
