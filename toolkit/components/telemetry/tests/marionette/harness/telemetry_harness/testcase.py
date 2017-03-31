# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import re
import simplejson as json
import zlib

from firefox_puppeteer import PuppeteerMixin
from marionette_harness import MarionetteTestCase
from marionette_harness.runner import httpd


here = os.path.abspath(os.path.dirname(__file__))
doc_root = os.path.join(os.path.dirname(here), "www")


class TelemetryTestCase(PuppeteerMixin, MarionetteTestCase):

    ping_list = []

    def setUp(self, *args, **kwargs):
        super(TelemetryTestCase, self).setUp()

        # Start and configure server
        self.httpd = httpd.FixtureServer(doc_root)
        ping_route = [("POST", re.compile('/pings'), self.pings)]
        self.httpd.routes.extend(ping_route)
        self.httpd.start()
        self.ping_server_url = '{}pings'.format(self.httpd.get_url('/'))

        telemetry_prefs = {
            'toolkit.telemetry.send.overrideOfficialCheck': True,
            'toolkit.telemetry.server': self.ping_server_url,
            'toolkit.telemetry.initDelay': '1',
            'datareporting.healthreport.uploadEnabled': True,
            'datareporting.policy.dataSubmissionEnabled': True,
            'datareporting.policy.dataSubmissionPolicyBypassNotification': True,
            'toolkit.telemetry.log.level': 0,
            'toolkit.telemetry.log.dump': True
        }

        # Firefox will be forced to restart with the prefs enforced.
        self.marionette.enforce_gecko_prefs(telemetry_prefs)

    def tearDown(self, *args, **kwargs):
        self.httpd.stop()
        super(TelemetryTestCase, self).tearDown()

    def pings(self, request, response):
        json_data = json.loads(unpack(request.headers, request.body))
        self.ping_list.append(json_data)
        return 200


def unpack(headers, data):
    if "Content-Encoding" in headers and headers["Content-Encoding"] == "gzip":
        return zlib.decompress(data, zlib.MAX_WBITS | 16)
    else:
        return data
