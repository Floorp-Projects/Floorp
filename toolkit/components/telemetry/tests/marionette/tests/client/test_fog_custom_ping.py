# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from telemetry_harness.fog_ping_filters import FOG_ONE_PING_ONLY_PING
from telemetry_harness.fog_testcase import FOGTestCase


class TestDeletionRequestPing(FOGTestCase):
    """Tests for the "one-ping-only" FOG custom ping."""

    def test_one_ping_only_ping(self):
        def send_opo_ping(marionette):
            ping_sending_script = "GleanPings.onePingOnly.submit();"
            with marionette.using_context(marionette.CONTEXT_CHROME):
                marionette.execute_script(ping_sending_script)

        ping1 = self.wait_for_ping(
            lambda: send_opo_ping(self.marionette),
            FOG_ONE_PING_ONLY_PING,
            ping_server=self.fog_ping_server,
        )

        self.assertNotIn("client_id", ping1["payload"]["client_info"])
