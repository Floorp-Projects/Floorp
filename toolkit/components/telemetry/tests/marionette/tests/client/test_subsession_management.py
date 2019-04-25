# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from telemetry_harness.testcase import TelemetryTestCase
from telemetry_harness.ping_filters import (
    MAIN_ENVIRONMENT_CHANGE_PING,
    MAIN_SHUTDOWN_PING,
)


class TestSubsessionManagement(TelemetryTestCase):
    """Tests for Firefox Telemetry subsession management."""

    def test_subsession_management(self):
        """Test for Firefox Telemetry subsession management."""

        # Session S1, subsession 1
        # Actions:
        # 1. Open browser
        # 2. Open a new tab
        # 3. Restart browser in new session

        with self.new_tab():
            # If Firefox Telemetry is working correctly, this will
            # be sufficient to record a tab open event.
            pass

        ping1 = self.wait_for_ping(self.restart_browser, MAIN_SHUTDOWN_PING)

        # Session S2, subsession 1
        # Outcome 1:
        # Received a main ping P1 for previous session
        # - Ping base contents:
        #     - clientId should be a valid UUID
        #     - reason should be "shutdown"
        #     - sessionId should be set
        #     - subsessionId should be set
        #     - previousSessionId should not be set
        #     - previousSubsessionId should not be set
        #     - subSessionCounter should be 1
        #     - profileSubSessionCounter should be 1
        # - Other ping contents:
        #     - tab_open_event_count in scalars

        client_id = ping1["clientId"]
        self.assertIsValidUUID(client_id)

        ping1_info = ping1["payload"]["info"]
        self.assertEqual(ping1_info["reason"], "shutdown")

        s1_session_id = ping1_info["sessionId"]
        self.assertNotEqual(s1_session_id, "")

        s1_s1_subsession_id = ping1_info["subsessionId"]
        self.assertNotEqual(s1_s1_subsession_id, "")
        self.assertIsNone(ping1_info["previousSessionId"])
        self.assertIsNone(ping1_info["previousSubsessionId"])
        self.assertEqual(ping1_info["subsessionCounter"], 1)
        self.assertEqual(ping1_info["profileSubsessionCounter"], 1)

        scalars1 = ping1["payload"]["processes"]["parent"]["scalars"]
        self.assertNotIn("browser.engagement.window_open_event_count", scalars1)
        self.assertEqual(scalars1["browser.engagement.tab_open_event_count"], 1)

        # Actions:
        # 1. Install addon

        ping2 = self.wait_for_ping(self.install_addon, MAIN_ENVIRONMENT_CHANGE_PING)

        [addon_id] = self.addon_ids  # Store the addon ID for verifying ping3 later

        # Session S2, subsession 2
        # Outcome 2:
        # Received a main ping P2 for previous subsession
        # - Ping base contents:
        #     - clientId should be set to the same value
        #     - sessionId should be set to a new value
        #     - subsessionId should be set to a new value
        #     - previousSessionId should be set to P1s sessionId value
        #     - previousSubsessionId should be set to P1s subsessionId value
        #     - subSessionCounter should be 1
        #     - profileSubSessionCounter should be 2
        #     - reason should be "environment-change"
        # - Other ping contents:
        #     - tab_open_event_count not in scalars

        self.assertEqual(ping2["clientId"], client_id)

        ping2_info = ping2["payload"]["info"]
        self.assertEqual(ping2_info["reason"], "environment-change")

        s2_session_id = ping2_info["sessionId"]
        self.assertNotEqual(s2_session_id, s1_session_id)

        s2_s1_subsession_id = ping2_info["subsessionId"]
        self.assertNotEqual(s2_s1_subsession_id, s1_s1_subsession_id)
        self.assertEqual(ping2_info["previousSessionId"], s1_session_id)
        self.assertEqual(ping2_info["previousSubsessionId"], s1_s1_subsession_id)
        self.assertEqual(ping2_info["subsessionCounter"], 1)
        self.assertEqual(ping2_info["profileSubsessionCounter"], 2)

        scalars2 = ping2["payload"]["processes"]["parent"]["scalars"]
        self.assertNotIn("browser.engagement.window_open_event_count", scalars2)
        self.assertNotIn("browser.engagement.tab_open_event_count", scalars2)

        # Actions
        # 1. Restart browser in new session

        ping3 = self.wait_for_ping(self.restart_browser, MAIN_SHUTDOWN_PING)

        # Session S3, subsession 1
        # Outcome 3:
        # Received a main ping P3 for session 2, subsession 2
        # - Ping base contents:
        #     - clientId should be set to the same value
        #     - sessionId should be set to P2s sessionId value
        #     - subsessionId should be set to a new value
        #     - previousSessionId should be set to P1s sessionId value
        #     - previousSubsessionId should be set to P2s subsessionId value
        #     - subSessionCounter should be 2
        #     - profileSubSessionCounter should be 3
        #     - reason should be "shutdown"
        # - Other ping contents:
        #     - addon ID in activeAddons in environment

        self.assertEqual(ping3["clientId"], client_id)

        ping3_info = ping3["payload"]["info"]
        self.assertEqual(ping3_info["reason"], "shutdown")

        self.assertEqual(ping3_info["sessionId"], s2_session_id)

        s2_s2_subsession_id = ping3_info["subsessionId"]
        self.assertNotEqual(s2_s2_subsession_id, s1_s1_subsession_id)
        self.assertNotEqual(s2_s2_subsession_id, s2_s1_subsession_id)
        self.assertEqual(ping3_info["previousSessionId"], s1_session_id)
        self.assertEqual(ping3_info["previousSubsessionId"], s2_s1_subsession_id)
        self.assertEqual(ping3_info["subsessionCounter"], 2)
        self.assertEqual(ping3_info["profileSubsessionCounter"], 3)

        scalars3 = ping3["payload"]["processes"]["parent"]["scalars"]
        self.assertNotIn("browser.engagement.window_open_event_count", scalars3)
        self.assertNotIn("browser.engagement.tab_open_event_count", scalars3)

        active_addons = ping3["environment"]["addons"]["activeAddons"]
        self.assertIn(addon_id, active_addons)
