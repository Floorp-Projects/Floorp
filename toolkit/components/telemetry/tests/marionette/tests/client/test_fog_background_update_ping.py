# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from telemetry_harness.fog_ping_filters import FOG_BACKGROUND_UPDATE_PING
from telemetry_harness.fog_testcase import FOGTestCase

import os
import subprocess


class TestBackgroundUpdatePing(FOGTestCase):
    """Tests for the "background-update" FOG custom ping.

    This test is subtle.  We launch Firefox to prepare a profile and to _disable_
    the background update setting.  We exit Firefox, leaving the (unlocked) profile
    to be used as the default profile for the background update task (and not having
    multiple instances running).  The task will not try to update, but it will send
    a ping.  Then we restart Firefox to unwind the background update setting and
    allow shutdown to proceed cleanly."""

    def test_background_update_ping(self):

        with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
            update_agent = self.marionette.execute_script(
                "return AppConstants.MOZ_UPDATE_AGENT;"
            )

        if not update_agent:
            # Before we skip this test, we need to quit marionette and the ping
            # server created in TelemetryTestCase by running tearDown.
            self.tearDown()
            self.skipTest("background update task is not enabled.")
            return

        def readUpdateConfigSetting(marionette, prefName):
            with marionette.using_context(marionette.CONTEXT_CHROME):
                return marionette.execute_async_script(
                    """
                let [prefName, resolve] = arguments;
                const { UpdateUtils } = ChromeUtils.import(
                  "resource://gre/modules/UpdateUtils.jsm"
                );
                UpdateUtils.readUpdateConfigSetting(prefName)
                  .then(resolve);
                """,
                    script_args=[prefName],
                )

        def writeUpdateConfigSetting(marionette, prefName, value, options=None):
            with marionette.using_context(marionette.CONTEXT_CHROME):
                marionette.execute_async_script(
                    """
                let [prefName, value, options, resolve] = arguments;
                const { UpdateUtils } = ChromeUtils.import(
                  "resource://gre/modules/UpdateUtils.jsm"
                );
                UpdateUtils.writeUpdateConfigSetting(prefName, value, options)
                  .then(() => Services.prefs.savePrefFile(null)) // Ensure flushed to disk!
                  .then(resolve);
                """,
                    script_args=[prefName, value, options],
                )

        def send_ping(marionette):
            env = dict(os.environ)
            env["MOZ_BACKGROUNDTASKS_DEFAULT_PROFILE_PATH"] = marionette.profile
            cmd = marionette.instance.runner.command[0]

            enabled = None
            with marionette.using_context(marionette.CONTEXT_CHROME):
                enabled = readUpdateConfigSetting(
                    marionette, "app.update.background.enabled"
                )
                writeUpdateConfigSetting(
                    marionette, "app.update.background.enabled", False
                )

            marionette.quit()
            try:
                subprocess.check_call(
                    [cmd, "--backgroundtask", "backgroundupdate"], env=env
                )

            finally:
                # Restart to reset our per-installation pref, which is on disk
                # on Windows and shared across profiles, so must be reset.
                # Also, this test needs a Marionette session in order to tear
                # itself down cleanly.
                marionette.start_session()
                if enabled is not None:
                    writeUpdateConfigSetting(
                        marionette, "app.update.background.enabled", enabled
                    )

        ping1 = self.wait_for_ping(
            lambda: send_ping(self.marionette),
            FOG_BACKGROUND_UPDATE_PING,
            ping_server=self.fog_ping_server,
        )

        # Check some background update details.
        self.assertEqual(
            True,
            ping1["payload"]["metrics"]["boolean"][
                "background_update.exit_code_success"
            ],
        )
        self.assertNotIn(
            "background_update.exit_code_exception",
            ping1["payload"]["metrics"]["boolean"],
        )
        self.assertEqual(
            "NEVER_CHECKED",
            ping1["payload"]["metrics"]["string"]["background_update.final_state"],
        )
        self.assertIn(
            "background_update.client_id", ping1["payload"]["metrics"]["uuid"]
        )
        self.assertEqual(
            "NEVER_CHECKED",
            ping1["payload"]["metrics"]["string"]["background_update.final_state"],
        )
        self.assertListEqual(
            ["NEVER_CHECKED"],
            ping1["payload"]["metrics"]["string_list"]["background_update.states"],
        )
        self.assertIn(
            "app.update.background.enabled=false",
            ping1["payload"]["metrics"]["string_list"]["background_update.reasons"],
        )

        # And make sure that the general update stuff is present.
        self.assertEqual(
            False, ping1["payload"]["metrics"]["boolean"]["update.background_update"]
        )
