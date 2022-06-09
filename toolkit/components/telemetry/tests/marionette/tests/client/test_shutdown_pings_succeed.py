# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from telemetry_harness.testcase import TelemetryTestCase


class TestShutdownPingsSucced(TelemetryTestCase):
    """Test Firefox shutdown pings."""

    def test_shutdown_pings_succeed(self):
        """Test that known Firefox shutdown pings are received."""

        ping_types = (
            "event",
            "first-shutdown",
            "main",
            "new-profile",
        )

        # We don't need the browser after this, but the harness expects a
        # running browser to clean up, so we `restart_browser` rather than
        # `quit_browser`.
        pings = self.wait_for_pings(
            self.restart_browser, lambda p: p["type"] in ping_types, len(ping_types)
        )

        self.assertEqual(len(pings), len(ping_types))
        self.assertEqual(set(ping_types), set(p["type"] for p in pings))
