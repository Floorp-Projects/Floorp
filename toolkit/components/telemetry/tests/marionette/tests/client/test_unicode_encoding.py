# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from telemetry_harness.ping_filters import MAIN_SHUTDOWN_PING
from telemetry_harness.testcase import TelemetryTestCase


class TestUnicodeEncoding(TelemetryTestCase):
    """Tests for Firefox Telemetry Unicode encoding."""

    def test_unicode_encoding(self):
        """Test for Firefox Telemetry Unicode encoding."""

        # We can use any string (not char!) pref to test the round-trip.
        pref = "app.support.baseURL"
        orig = "€ —"
        with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
            value = self.marionette.execute_script(
                r"""
                Services.prefs.setStringPref("{pref}", "{orig}");
                return Services.prefs.getStringPref("{pref}");
                """.format(
                    orig=orig,
                    pref=pref,
                )
            )

        self.assertEqual(value, orig)

        # We don't need the browser after this, but the harness expects a
        # running browser to clean up, so we `restart_browser` rather than
        # `quit_browser`.
        ping1 = self.wait_for_ping(self.restart_browser, MAIN_SHUTDOWN_PING)

        self.assertEqual(
            ping1["environment"]["settings"]["userPrefs"][pref],
            orig,
        )
