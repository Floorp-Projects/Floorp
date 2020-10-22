# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from telemetry_harness.testcase import TelemetryTestCase
from telemetry_harness.ping_filters import MAIN_SHUTDOWN_PING


class TestMainTabScalars(TelemetryTestCase):
    """Tests for Telemetry Scalars."""

    def test_main_tab_scalars(self):
        """Test for Telemetry Scalars."""

        with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
            start_tab = self.marionette.current_window_handle

            tab2 = self.open_tab(focus=True)
            self.marionette.switch_to_window(tab2)

            tab3 = self.open_tab(focus=True)
            self.marionette.switch_to_window(tab3)

            self.marionette.close()
            self.marionette.switch_to_window(tab2)

            self.marionette.close()
            self.marionette.switch_to_window(start_tab)

        ping = self.wait_for_ping(self.restart_browser, MAIN_SHUTDOWN_PING)

        self.assertEqual(ping["type"], "main")
        self.assertEqual(ping["clientId"], self.client_id)

        scalars = ping["payload"]["processes"]["parent"]["scalars"]

        self.assertEqual(
            scalars["browser.engagement.max_concurrent_tab_count"], 3
        )
        self.assertEqual(scalars["browser.engagement.tab_open_event_count"], 2)
        self.assertEqual(
            scalars["browser.engagement.max_concurrent_window_count"], 1
        )
