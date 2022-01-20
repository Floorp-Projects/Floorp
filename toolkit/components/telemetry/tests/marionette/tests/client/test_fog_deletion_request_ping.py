# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import textwrap

from telemetry_harness.fog_ping_filters import FOG_DELETION_REQUEST_PING
from telemetry_harness.fog_testcase import FOGTestCase


class TestDeletionRequestPing(FOGTestCase):
    """Tests for FOG deletion-request ping."""

    def test_deletion_request_ping_across_sessions(self):
        """Test the "deletion-request" ping behaviour across sessions."""

        self.search_in_new_tab("mozilla firefox")

        ping1 = self.wait_for_ping(
            self.disable_telemetry,
            FOG_DELETION_REQUEST_PING,
            ping_server=self.fog_ping_server,
        )

        self.assertIn("ping_info", ping1["payload"])
        self.assertIn("client_info", ping1["payload"])

        self.assertIn("client_id", ping1["payload"]["client_info"])
        client_id1 = ping1["payload"]["client_info"]["client_id"]
        self.assertIsValidUUID(client_id1)

        self.restart_browser()

        self.assertEqual(self.fog_ping_server.pings[-1], ping1)

        self.enable_telemetry()
        self.restart_browser()

        debug_tag = "my-test-tag"
        tagging_script = """\
        let FOG = Components.classes["@mozilla.org/toolkit/glean;1"]
            .createInstance(Components.interfaces.nsIFOG);
        FOG.setTagPings("{}");
        """.format(
            debug_tag
        )
        with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
            self.marionette.execute_script(textwrap.dedent(tagging_script))
        self.search_in_new_tab("python unittest")

        ping2 = self.wait_for_ping(
            self.disable_telemetry,
            FOG_DELETION_REQUEST_PING,
            ping_server=self.fog_ping_server,
        )

        self.assertEqual(ping2["debug_tag"].decode("utf-8"), debug_tag)

        self.assertIn("client_id", ping2["payload"]["client_info"])
        client_id2 = ping2["payload"]["client_info"]["client_id"]
        self.assertIsValidUUID(client_id2)

        # Verify that FOG creates a new client ID when a user
        # opts out of sending technical and interaction data.
        self.assertNotEqual(client_id2, client_id1)
