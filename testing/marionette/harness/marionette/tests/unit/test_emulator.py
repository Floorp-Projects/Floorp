# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from unittest import skip

from marionette.marionette_test import MarionetteTestCase, skip_if_desktop, skip_unless_protocol
from marionette_driver.errors import MarionetteException, JavascriptException


class TestEmulatorContent(MarionetteTestCase):
    @skip_if_desktop
    def test_emulator_cmd(self):
        self.marionette.set_script_timeout(10000)
        expected = ["<build>", "OK"]
        res = self.marionette.execute_async_script(
            "runEmulatorCmd('avd name', marionetteScriptFinished)");
        self.assertEqual(res, expected)

    @skip_if_desktop
    def test_emulator_shell(self):
        self.marionette.set_script_timeout(10000)
        expected = ["Hello World!"]
        res = self.marionette.execute_async_script(
            "runEmulatorShell(['echo', 'Hello World!'], marionetteScriptFinished)")
        self.assertEqual(res, expected)

    @skip_if_desktop
    def test_emulator_order(self):
        self.marionette.set_script_timeout(10000)
        with self.assertRaises(MarionetteException):
            self.marionette.execute_async_script("""
               runEmulatorCmd("gsm status", function(res) {});
               marionetteScriptFinished(true);
               """)


class TestEmulatorChrome(TestEmulatorContent):
    def setUp(self):
        super(TestEmulatorChrome, self).setUp()
        self.marionette.set_context("chrome")


class TestEmulatorScreen(MarionetteTestCase):
    @skip_if_desktop
    def test_emulator_orientation(self):
        self.screen = self.marionette.emulator.screen
        self.screen.initialize()

        self.assertEqual(self.screen.orientation, self.screen.SO_PORTRAIT_PRIMARY,
                         'Orientation has been correctly initialized.')

        self.screen.orientation = self.screen.SO_PORTRAIT_SECONDARY
        self.assertEqual(self.screen.orientation, self.screen.SO_PORTRAIT_SECONDARY,
                         'Orientation has been set to portrait-secondary')

        self.screen.orientation = self.screen.SO_LANDSCAPE_PRIMARY
        self.assertEqual(self.screen.orientation, self.screen.SO_LANDSCAPE_PRIMARY,
                         'Orientation has been set to landscape-primary')

        self.screen.orientation = self.screen.SO_LANDSCAPE_SECONDARY
        self.assertEqual(self.screen.orientation, self.screen.SO_LANDSCAPE_SECONDARY,
                         'Orientation has been set to landscape-secondary')

        self.screen.orientation = self.screen.SO_PORTRAIT_PRIMARY
        self.assertEqual(self.screen.orientation, self.screen.SO_PORTRAIT_PRIMARY,
                         'Orientation has been set to portrait-primary')


class TestEmulatorCallbacks(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.original_emulator_cmd = self.marionette._emulator_cmd
        self.original_emulator_shell = self.marionette._emulator_shell
        self.marionette._emulator_cmd = self.mock_emulator_cmd
        self.marionette._emulator_shell = self.mock_emulator_shell

    def tearDown(self):
        self.marionette._emulator_cmd = self.original_emulator_cmd
        self.marionette._emulator_shell = self.original_emulator_shell

    def mock_emulator_cmd(self, *args):
        return self.marionette._send_emulator_result("cmd response")

    def mock_emulator_shell(self, *args):
        return self.marionette._send_emulator_result("shell response")

    def _execute_emulator(self, action, args):
        script = "%s(%s, function(res) { marionetteScriptFinished(res); })" % (action, args)
        return self.marionette.execute_async_script(script)

    def emulator_cmd(self, cmd):
        return self._execute_emulator("runEmulatorCmd", escape(cmd))

    def emulator_shell(self, *args):
        js_args = ", ".join(map(escape, args))
        js_args = "[%s]" % js_args
        return self._execute_emulator("runEmulatorShell", js_args)

    def test_emulator_cmd_content(self):
        with self.marionette.using_context("content"):
            res = self.emulator_cmd("yo")
            self.assertEqual("cmd response", res)

    def test_emulator_shell_content(self):
        with self.marionette.using_context("content"):
            res = self.emulator_shell("first", "second")
            self.assertEqual("shell response", res)

    @skip_unless_protocol(lambda level: level >= 3)
    def test_emulator_result_error_content(self):
        with self.marionette.using_context("content"):
            with self.assertRaisesRegexp(JavascriptException, "TypeError"):
                self.marionette.execute_async_script("runEmulatorCmd()")

    def test_emulator_cmd_chrome(self):
        with self.marionette.using_context("chrome"):
            res = self.emulator_cmd("yo")
            self.assertEqual("cmd response", res)

    def test_emulator_shell_chrome(self):
        with self.marionette.using_context("chrome"):
            res = self.emulator_shell("first", "second")
            self.assertEqual("shell response", res)

    @skip_unless_protocol(lambda level: level >= 3)
    def test_emulator_result_error_chrome(self):
        with self.marionette.using_context("chrome"):
            with self.assertRaisesRegexp(JavascriptException, "TypeError"):
                self.marionette.execute_async_script("runEmulatorCmd()")

    def test_multiple_callbacks(self):
        res = self.marionette.execute_async_script("""
            runEmulatorCmd("what");
            runEmulatorCmd("ho");
            marionetteScriptFinished("Frobisher");
            """)
        self.assertEqual("Frobisher", res)

    # This should work, but requires work on emulator callbacks:
    """
    def test_multiple_nested_callbacks(self):
        res = self.marionette.execute_async_script('''
            runEmulatorCmd("what", function(res) {
              runEmulatorCmd("ho", marionetteScriptFinished);
            });''')
        self.assertEqual("cmd response", res)
    """


def escape(word):
    return "'%s'" % word
