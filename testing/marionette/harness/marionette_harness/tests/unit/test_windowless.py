# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver import errors, Wait
from marionette_harness import MarionetteTestCase


class TestWindowless(MarionetteTestCase):
    def setUp(self):
        super(TestWindowless, self).setUp()

        self.marionette.delete_session()
        self.marionette.start_session({"moz:windowless": True})

    def tearDown(self):
        # Reset the browser and active WebDriver session
        self.marionette.restart(in_app=True)
        self.marionette.delete_session()

        super(TestWindowless, self).tearDown()

    def wait_for_first_window(self):
        wait = Wait(
            self.marionette,
            ignored_exceptions=errors.NoSuchWindowException,
            timeout=5,
        )
        return wait.until(lambda _: self.marionette.window_handles)

    def test_last_chrome_window_can_be_closed(self):
        with self.marionette.using_context("chrome"):
            handles = self.marionette.chrome_window_handles
            self.assertGreater(len(handles), 0)
            self.marionette.switch_to_window(handles[0])
            self.marionette.close_chrome_window()
            self.assertEqual(len(self.marionette.chrome_window_handles), 0)

    def test_last_content_window_can_be_closed(self):
        handles = self.marionette.window_handles
        self.assertGreater(len(handles), 0)
        self.marionette.switch_to_window(handles[0])
        self.marionette.close()
        self.assertEqual(len(self.marionette.window_handles), 0)

    def test_no_window_handles_after_silent_restart(self):
        # Check that windows are present, but not after a silent restart
        handles = self.marionette.window_handles
        self.assertGreater(len(handles), 0)

        self.marionette.restart(silent=True)
        with self.assertRaises(errors.TimeoutException):
            self.wait_for_first_window()

        # After a normal restart a browser window will be opened again
        self.marionette.restart(in_app=True)
        handles = self.wait_for_first_window()

        self.assertGreater(len(handles), 0)
        self.marionette.switch_to_window(handles[0])
