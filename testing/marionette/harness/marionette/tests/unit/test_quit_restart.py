# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase
from marionette_driver.errors import MarionetteException


class TestQuitRestart(MarionetteTestCase):

    def setUp(self):
        MarionetteTestCase.setUp(self)

        self.pid = self.marionette.session["processId"]
        self.session_id = self.marionette.session_id

        self.assertNotEqual(self.marionette.get_pref("browser.startup.page"), 3)
        self.marionette.set_pref("browser.startup.page", 3)

    def tearDown(self):
        # Ensure to restart a session if none exist for clean-up
        if not self.marionette.session:
            self.marionette.start_session()

        self.marionette.clear_pref("browser.startup.page")

        MarionetteTestCase.tearDown(self)

    def test_force_restart(self):
        self.marionette.restart()
        self.assertEqual(self.marionette.session_id, self.session_id)

        # A forced restart will cause a new process id
        self.assertNotEqual(self.marionette.session["processId"], self.pid)

        # If a preference value is not forced, a restart will cause a reset
        self.assertNotEqual(self.marionette.get_pref("browser.startup.page"), 3)

    def test_in_app_restart(self):
        self.marionette.restart(in_app=True)
        self.assertEqual(self.marionette.session_id, self.session_id)

        # An in-app restart will keep the same process id only on Linux
        if self.marionette.session_capabilities['platformName'] == 'linux':
            self.assertEqual(self.marionette.session["processId"], self.pid)
        else:
            self.assertNotEqual(self.marionette.session["processId"], self.pid)

        # If a preference value is not forced, a restart will cause a reset
        self.assertNotEqual(self.marionette.get_pref("browser.startup.page"), 3)

    def test_in_app_clean_restart(self):
        with self.assertRaises(ValueError):
            self.marionette.restart(in_app=True, clean=True)

    def test_force_quit(self):
        self.marionette.quit()

        self.assertEqual(self.marionette.session, None)
        with self.assertRaisesRegexp(MarionetteException, "Please start a session"):
            self.marionette.get_url()

        self.marionette.start_session()
        self.assertNotEqual(self.marionette.session_id, self.session_id)
        self.assertNotEqual(self.marionette.get_pref("browser.startup.page"), 3)

    def test_in_app_quit(self):
        self.marionette.quit(in_app=True)

        self.assertEqual(self.marionette.session, None)
        with self.assertRaisesRegexp(MarionetteException, "Please start a session"):
            self.marionette.get_url()

        self.marionette.start_session()
        self.assertNotEqual(self.marionette.session_id, self.session_id)
        self.assertNotEqual(self.marionette.get_pref("browser.startup.page"), 3)
