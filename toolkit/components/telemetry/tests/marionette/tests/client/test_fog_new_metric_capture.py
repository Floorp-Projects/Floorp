# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from telemetry_harness.fog_ping_filters import FOG_NODC_PING
from telemetry_harness.fog_testcase import FOGTestCase


class TestNewMetricCaptureEmulation(FOGTestCase):
    """Test for the New Metric Capture Emulation Ping (aka NODC)"""

    @staticmethod
    def user_active(active, marionette):
        script = (
            "Services.obs.notifyObservers(null, 'user-interaction-{}active')".format(
                "" if active else "in"
            )
        )
        with marionette.using_context(marionette.CONTEXT_CHROME):
            marionette.execute_script(script)

    def test_user_activity(self):
        # tests send on active, inactive and restart
        with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
            zero_prefs_script = """\
            Services.prefs.setIntPref("telemetry.fog.test.inactivity_limit", 0);
            Services.prefs.setIntPref("telemetry.fog.test.activity_limit", 0);
            """
            self.marionette.execute_script(zero_prefs_script)

        active_ping = self.wait_for_ping(
            lambda: self.user_active(True, self.marionette),
            FOG_NODC_PING,
            ping_server=self.fog_ping_server,
        )

        inactive_ping = self.wait_for_ping(
            lambda: self.user_active(False, self.marionette),
            FOG_NODC_PING,
            ping_server=self.fog_ping_server,
        )

        self.assertEqual("active", active_ping["payload"]["ping_info"]["reason"])
        self.assertEqual("inactive", inactive_ping["payload"]["ping_info"]["reason"])

        # Restarting the browser sends the experiment ping with
        # reason "active". One should be sent with this reason on initialization
        # of FOG, the same way it would for baseline. This has to go last.

        fog_init_ping = self.wait_for_ping(
            self.restart_browser, FOG_NODC_PING, ping_server=self.fog_ping_server
        )
        self.assertEqual("active", fog_init_ping["payload"]["ping_info"]["reason"])
        return
