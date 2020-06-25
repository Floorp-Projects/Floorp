# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import shutil
import tempfile

import mozlog
from telemetry_harness.fog_ping_server import FOGPingServer
from telemetry_harness.testcase import TelemetryTestCase


class FOGTestCase(TelemetryTestCase):
    """Base testcase class for project FOG."""

    def __init__(self, *args, **kwargs):
        """Initialize the test case and create a ping server."""
        super(FOGTestCase, self).__init__(*args, **kwargs)
        self._logger = mozlog.get_default_logger(component="FOGTestCase")

    def setUp(self, *args, **kwargs):
        """Set up the test case and create a FOG ping server.

        This test is skipped if the build doesn't support FOG.
        """
        super(FOGTestCase, self).setUp(*args, **kwargs)

        with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
            fog_present = self.marionette.execute_script("return AppConstants.NIGHTLY_BUILD;")

        if not fog_present:
            # Before we skip this test, we need to quit marionette and the ping
            # server created in TelemetryTestCase by running tearDown
            super(FOGTestCase, self).tearDown(*args, **kwargs)
            self.skipTest("FOG is only present in AppConstants.NIGHTLY_BUILD builds.")

        self.fog_ping_server = FOGPingServer(
            self.testvars["server_root"], "http://localhost:0"
        )
        self.fog_ping_server.start()

        self._logger.info(
            "Submitting to FOG ping server at {}".format(self.fog_ping_server.url)
        )

        # Make sure to escape the fog_data_path to avoid Unicode character
        # escape sequence errors on Windows when setting Gecko preferences
        self.fog_data_path = os.path.abspath(tempfile.mkdtemp()).encode("string-escape")
        self._logger.info("Using FOG data_path {}".format(self.fog_data_path))

        self.marionette.enforce_gecko_prefs(
            {
                "telemetry.fog.temporary_and_just_for_testing.data_path": self.fog_data_path,
                "telemetry.fog.test.localhost_port": self.fog_ping_server.port,
            }
        )

    def tearDown(self, *args, **kwargs):
        super(FOGTestCase, self).tearDown(*args, **kwargs)
        self.fog_ping_server.stop()
        self._logger.info("Removing FOG data_path {}".format(self.fog_data_path))
        shutil.rmtree(self.fog_data_path)
