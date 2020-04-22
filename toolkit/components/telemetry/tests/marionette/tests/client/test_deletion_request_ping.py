# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from telemetry_harness.testcase import TelemetryTestCase
from telemetry_harness.ping_filters import ANY_PING, DELETION_REQUEST_PING, MAIN_SHUTDOWN_PING


class TestDeletionRequestPing(TelemetryTestCase):
    """Tests for "deletion-request" ping."""

    def test_deletion_request_ping_across_sessions(self):
        """Test the "deletion-request" ping behaviour across sessions."""

        # Get the client_id.
        client_id = self.wait_for_ping(self.install_addon, ANY_PING)["clientId"]
        self.assertIsValidUUID(client_id)

        # Trigger an "deletion-request" ping.
        ping = self.wait_for_ping(self.disable_telemetry, DELETION_REQUEST_PING)

        self.assertIn("clientId", ping)
        self.assertIn("payload", ping)
        self.assertNotIn("environment", ping["payload"])

        # Close Firefox cleanly.
        self.quit_browser()

        # TODO: Check pending pings aren't persisted

        # Start Firefox.
        self.start_browser()

        # Trigger an environment change, which isn't allowed to send a ping.
        self.install_addon()

        # Ensure we've sent no pings since "disabling telemetry".
        self.assertEqual(self.ping_server.pings[-1], ping)

        # Turn Telemetry back on.
        self.enable_telemetry()

        # Close Firefox cleanly, collecting its "main"/"shutdown" ping.
        main_ping = self.wait_for_ping(self.restart_browser, MAIN_SHUTDOWN_PING)

        # Ensure the "main" ping has changed its client id.
        self.assertIn("clientId", main_ping)
        self.assertIsValidUUID(main_ping["clientId"])
        self.assertNotEqual(main_ping["clientId"], client_id)

        # Ensure we note in the ping that the user opted in.
        parent_scalars = main_ping["payload"]["processes"]["parent"]["scalars"]

        self.assertIn("telemetry.data_upload_optin", parent_scalars)
        self.assertIs(parent_scalars["telemetry.data_upload_optin"], True)

        # Ensure all pings sent during this test don't have the c0ffee client id.
        for ping in self.ping_server.pings:
            if "clientId" in ping:
                self.assertIsValidUUID(ping["clientId"])
