# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from telemetry_harness.testcase import TelemetryTestCase
from telemetry_harness.ping_filters import (
    MAIN_SHUTDOWN_PING,
    MAIN_ENVIRONMENT_CHANGE_PING,
)


class TestDynamicProbes(TelemetryTestCase):
    """Tests for Dynamic Probes."""

    def test_dynamic_probes(self):
        """Test for dynamic probes."""
        self.wait_for_ping(self.install_dynamic_addon, MAIN_ENVIRONMENT_CHANGE_PING)

        ping = self.wait_for_ping(self.restart_browser, MAIN_SHUTDOWN_PING)

        [addon_id] = self.addon_ids  # check addon id exists
        active_addons = ping["environment"]["addons"]["activeAddons"]
        self.assertIn(addon_id, active_addons)

        scalars = ping["payload"]["processes"]["dynamic"]["scalars"]
        self.assertEqual(scalars["dynamic.probe.counter_scalar"], 1337)
