# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_harness import MarionetteTestCase


class TestLog(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        # clears log cache
        self.marionette.get_logs()

    def test_log(self):
        self.marionette.log("foo")
        logs = self.marionette.get_logs()
        self.assertEqual("INFO", logs[0][0])
        self.assertEqual("foo", logs[0][1])

    def test_log_level(self):
        self.marionette.log("foo", "ERROR")
        logs = self.marionette.get_logs()
        self.assertEqual("ERROR", logs[0][0])
        self.assertEqual("foo", logs[0][1])

    def test_clear(self):
        self.marionette.log("foo")
        self.assertEqual(1, len(self.marionette.get_logs()))
        self.assertEqual(0, len(self.marionette.get_logs()))

    def test_multiple_entries(self):
        self.marionette.log("foo")
        self.marionette.log("bar")
        self.assertEqual(2, len(self.marionette.get_logs()))

    def test_log_from_sync_script(self):
        self.marionette.execute_script("log('foo')")
        logs = self.marionette.get_logs()
        self.assertEqual("INFO", logs[0][0])
        self.assertEqual("foo", logs[0][1])

    def test_log_from_sync_script_level(self):
        self.marionette.execute_script("log('foo', 'ERROR')")
        logs = self.marionette.get_logs()
        self.assertEqual("ERROR", logs[0][0])
        self.assertEqual("foo", logs[0][1])

    def test_log_from_async_script(self):
        self.marionette.execute_async_script("log('foo'); arguments[0]();")
        logs = self.marionette.get_logs()
        self.assertEqual("INFO", logs[0][0])
        self.assertEqual("foo", logs[0][1])

    def test_log_from_async_script_variable_arguments(self):
        self.marionette.execute_async_script("log('foo', 'ERROR'); arguments[0]();")
        logs = self.marionette.get_logs()
        self.assertEqual("ERROR", logs[0][0])
        self.assertEqual("foo", logs[0][1])


class TestLogChrome(TestLog):
    def setUp(self):
        TestLog.setUp(self)
        self.marionette.set_context("chrome")
