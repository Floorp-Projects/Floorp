# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from marionette_test import MarionetteTestCase

class TestLog(MarionetteTestCase):
    def test_log_basic(self):
        # clear any previous data
        self.marionette.get_logs()

        self.marionette.log("I am info")
        self.assertTrue("I am info" in self.marionette.get_logs()[0])
        self.marionette.log("I AM ERROR", "ERROR")
        self.assertTrue("I AM ERROR" in self.marionette.get_logs()[0])

    def test_that_we_can_clear_the_logs(self):
        # clear any previous data
        self.marionette.get_logs()

        self.marionette.log("I am info")
        self.assertTrue("I am info" in self.marionette.get_logs()[0])
        self.marionette.log("I AM ERROR", "ERROR")
        self.assertTrue("I AM ERROR" in self.marionette.get_logs()[0])

        # Check that is empty if we call it again
        self.assertEqual(0, len(self.marionette.get_logs()))

    def test_log_script(self):
        # clear any previous data
        self.marionette.get_logs()

        self.marionette.execute_script("log('some log');")
        self.assertTrue("some log" in self.marionette.get_logs()[0])
        self.marionette.execute_script("log('some error', 'ERROR');")
        self.assertTrue("some error" in self.marionette.get_logs()[0])
        self.marionette.set_script_timeout(2000)
        self.marionette.execute_async_script("log('some more logs'); finish();")
        self.assertTrue("some more logs" in self.marionette.get_logs()[0])
        self.marionette.execute_async_script("log('some more errors', 'ERROR'); finish();")
        self.assertTrue("some more errors" in self.marionette.get_logs()[0])

class TestLogChrome(TestLog):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.set_context("chrome")
