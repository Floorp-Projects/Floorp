# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from telemetry_harness.fog_ping_filters import FOG_BASELINE_PING
from telemetry_harness.fog_testcase import FOGTestCase


class TestClientActivity(FOGTestCase):
    """Tests for client activity and FOG's scheduling of the "baseline" ping."""

    def test_user_activity(self):
        # First test that restarting the browser sends a "active" ping
        ping0 = self.wait_for_ping(
            self.restart_browser, FOG_BASELINE_PING, ping_server=self.fog_ping_server
        )
        self.assertEqual("active", ping0["payload"]["ping_info"]["reason"])

        with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
            zero_prefs_script = """\
            Services.prefs.setIntPref("telemetry.fog.test.inactivity_limit", 0);
            Services.prefs.setIntPref("telemetry.fog.test.activity_limit", 0);
            """
            self.marionette.execute_script(zero_prefs_script)

        def user_active(active, marionette):
            script = "Services.obs.notifyObservers(null, 'user-interaction-{}active')".format(
                "" if active else "in"
            )
            with marionette.using_context(marionette.CONTEXT_CHROME):
                marionette.execute_script(script)

        ping1 = self.wait_for_ping(
            lambda: user_active(True, self.marionette),
            FOG_BASELINE_PING,
            ping_server=self.fog_ping_server,
        )

        ping2 = self.wait_for_ping(
            lambda: user_active(False, self.marionette),
            FOG_BASELINE_PING,
            ping_server=self.fog_ping_server,
        )

        self.assertEqual("active", ping1["payload"]["ping_info"]["reason"])
        self.assertEqual("inactive", ping2["payload"]["ping_info"]["reason"])
