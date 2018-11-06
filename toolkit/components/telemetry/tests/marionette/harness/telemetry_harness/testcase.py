# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import simplejson as json
import time
import zlib

from multiprocessing import Process

from firefox_puppeteer import PuppeteerMixin
from marionette_driver.addons import Addons
from marionette_driver.errors import MarionetteException
from marionette_driver.wait import Wait
from marionette_harness import MarionetteTestCase
from marionette_harness.runner import httpd


class TelemetryTestCase(PuppeteerMixin, MarionetteTestCase):

    def __init__(self, *args, **kwargs):
        super(TelemetryTestCase, self).__init__(*args, **kwargs)
        self.pings = []

    def setUp(self, *args, **kwargs):
        super(TelemetryTestCase, self).setUp(*args, **kwargs)

        @httpd.handlers.handler
        def pings_handler(request, response):
            """Handler for HTTP requests to the ping server."""

            request_data = request.body

            if request.headers.get("Content-Encoding") == "gzip":
                request_data = zlib.decompress(request_data, zlib.MAX_WBITS | 16)

            ping_data = json.loads(request_data)

            # Store JSON data to self.pings to be used by wait_for_pings()
            self.pings.append(ping_data)

            ping_type = ping_data["type"]

            log_message = "pings_handler received '{}' ping".format(ping_type)

            if ping_type == "main":
                ping_reason = ping_data["payload"]["info"]["reason"]
                log_message = "{} with reason '{}'".format(log_message, ping_reason)

            self.logger.info(log_message)

            status_code = 200
            content = "OK"
            headers = [
                ("Content-Type", "text/plain"),
                ("Content-Length", len(content)),
            ]

            return (status_code, headers, content)

        self.httpd = httpd.FixtureServer(self.testvars['server_root'])
        self.httpd.router.register("POST", '/pings*', pings_handler)
        self.httpd.start()

        self.ping_server_url = '{}pings'.format(self.httpd.get_url('/'))

        telemetry_prefs = {
            'toolkit.telemetry.server': self.ping_server_url,
            'toolkit.telemetry.initDelay': 1,
            'toolkit.telemetry.minSubsessionLength': 0,
            'datareporting.healthreport.uploadEnabled': True,
            'datareporting.policy.dataSubmissionEnabled': True,
            'datareporting.policy.dataSubmissionPolicyBypassNotification': True,
            'toolkit.telemetry.log.level': 0,
            'toolkit.telemetry.log.dump': True,
            'toolkit.telemetry.send.overrideOfficialCheck': True
        }

        # Firefox will be forced to restart with the prefs enforced.
        self.marionette.enforce_gecko_prefs(telemetry_prefs)

        # Wait 5 seconds to ensure that telemetry has reinitialized
        time.sleep(5)

    def wait_for_pings(self, action_func, ping_filter_func, count):
        """Call the given action and wait for pings to come in and return
        the `count` number of pings, that match the given filter.
        """
        # Keep track of the current number of pings
        current_num_pings = len(self.pings)

        # New list to store new pings that satisfy the filter
        filtered_pings = []

        def wait_func(*args, **kwargs):
            # Ignore existing pings in self.pings
            new_pings = self.pings[current_num_pings:]

            # Filter pings to make sure we wait for the correct ping type
            filtered_pings[:] = [p for p in new_pings if ping_filter_func(p)]

            return len(filtered_pings) >= count

        self.logger.info(
            "wait_for_pings running action '{action}'.".format(
                action=action_func.__name__,
            )
        )

        # Call given action and wait for a ping
        action_func()

        try:
            Wait(self.marionette, 60).until(wait_func)
        except Exception as e:
            self.fail("Error waiting for ping: {}".format(e.message))

        return filtered_pings[:count]

    def restart_browser(self):
        """Restarts browser while maintaining the same profile and session."""
        self.restart(clean=False, in_app=True)

    def install_addon(self):
        trigger = Process(target=self._install_addon)
        trigger.start()

    def _install_addon(self):
        # The addon that gets installed here is the easyscreenshot addon taken from AMO.
        # It has high compatibility with firefox and doesn't cause any adverse side affects that
        # could affect our tests like tabs opening, etc.
        # Developed by: MozillaOnline
        # Addon URL: https://addons.mozilla.org/en-US/firefox/addon/easyscreenshot/
        try:
            # TODO: Replace Resources_dir with default directory
            addon_path = os.path.join('resources_dir', 'easyscreenshot.xpi')
            addons = Addons(self.marionette)
            addons.install(addon_path)
        except MarionetteException as e:
            self.fail('{} - Error installing addon: {} - '.format(e.cause, e.message))

    @property
    def client_id(self):
        return self.marionette.execute_script('Cu.import("resource://gre/modules/ClientID.jsm");'
                                              'return ClientID.getCachedClientID();')

    @property
    def subsession_id(self):
        ping_data = self.marionette.execute_script(
            'Cu.import("resource://gre/modules/TelemetryController.jsm");'
            'return TelemetryController.getCurrentPingData(true);')
        return ping_data[u'payload'][u'info'][u'subsessionId']

    def tearDown(self, *args, **kwargs):
        self.httpd.stop()
        super(TelemetryTestCase, self).tearDown()
