# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_harness import MarionetteTestCase


class TestFirstStartup(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.quit()
        self.marionette.instance.prefs = {
            "browser.startup.homepage_override.mstone": ""
        }
        self.marionette.instance.app_args = ["-first-startup"]

    def test_new_profile(self):
        """Test launching with --first-startup when a new profile was created.

        Launches the browser with --first-startup on a freshly created profile
        and then ensures that FirstStartup.init ran successfully.
        """

        self.marionette.instance.switch_profile()
        self.marionette.start_session()
        self.marionette.set_context("chrome")
        firstStartupInittedSuccessfully = self.marionette.execute_script(
            """
          const { FirstStartup } = ChromeUtils.importESModule("resource://gre/modules/FirstStartup.sys.mjs");
          return FirstStartup.state == FirstStartup.SUCCESS
        """
        )

        self.assertTrue(
            firstStartupInittedSuccessfully, "FirstStartup initted successfully"
        )

    def test_existing_profile(self):
        """Test launching with --first-startup with a pre-existing profile.

        Launches the browser with --first-startup on a profile that has been
        run before. Ensures that FirstStartup.init was never run.
        """

        self.marionette.start_session()
        self.marionette.set_context("chrome")
        firstStartupSkipped = self.marionette.execute_script(
            """
          const { FirstStartup } = ChromeUtils.importESModule("resource://gre/modules/FirstStartup.sys.mjs");
          return FirstStartup.state == FirstStartup.NOT_STARTED
        """
        )

        self.assertTrue(firstStartupSkipped, "FirstStartup init skipped")
