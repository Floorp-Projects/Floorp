# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from telemetry_harness.testcase import TelemetryTestCase
from telemetry_harness.ping_filters import ANY_PING, OPTOUT_PING, FIRST_SHUTDOWN_PING


class TestOptoutPing(TelemetryTestCase):
    """Tests for "optout" ping."""

    def disable_telemetry(self):
        self.marionette.set_pref("datareporting.healthreport.uploadEnabled", False)

    def enable_telemetry(self):
        self.marionette.set_pref("datareporting.healthreport.uploadEnabled", True)

    def test_optout_ping(self):
        """Test for "optout" ping."""

        # Get the client_id.
        client_id = self.wait_for_ping(self.install_addon, ANY_PING)["clientId"]
        self.assertIsValidUUID(client_id)

        # Trigger an "optout" ping and check its contents.
        optout_ping = self.wait_for_ping(self.disable_telemetry, OPTOUT_PING)

        self.assertIsNotNone(optout_ping)
        self.assertNotIn("clientId", optout_ping)
        self.assertIn("payload", optout_ping)
        self.assertNotIn("environment", optout_ping["payload"])

        # Ensure the "optout" ping was the last ping sent
        self.assertEqual(self.ping_server.pings[-1], optout_ping)

        """ Test what happens when the user opts back in """

        self.enable_telemetry()

        # Restart Firefox so we can capture a "first-shutdown" ping.
        # We want to check that the client id changed and telemetry.data_upload_optin is set.
        shutdown_ping = self.wait_for_ping(self.restart_browser, FIRST_SHUTDOWN_PING)

        self.assertIn("clientId", shutdown_ping)
        self.assertIsValidUUID(shutdown_ping["clientId"])
        self.assertNotEqual(shutdown_ping["clientId"], client_id)

        self.assertIn("payload", shutdown_ping)
        self.assertIn("processes", shutdown_ping["payload"])
        self.assertIn("parent", shutdown_ping["payload"]["processes"])
        self.assertIn("scalars", shutdown_ping["payload"]["processes"]["parent"])
        scalars = shutdown_ping["payload"]["processes"]["parent"]["scalars"]

        self.assertIn("telemetry.data_upload_optin", scalars)
        self.assertTrue(scalars["telemetry.data_upload_optin"])
