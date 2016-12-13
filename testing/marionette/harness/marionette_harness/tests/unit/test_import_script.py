# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

from marionette_driver.by import By
from marionette_driver.errors import JavascriptException

from marionette_harness import MarionetteTestCase, WindowManagerMixin, skip_if_chrome


class TestImportScriptContent(WindowManagerMixin, MarionetteTestCase):
    contexts = set(["chrome", "content"])

    script_file = os.path.abspath(
        os.path.join(__file__, os.path.pardir, "importscript.js"))
    another_script_file = os.path.abspath(
        os.path.join(__file__, os.path.pardir, "importanotherscript.js"))

    def setUp(self):
        super(TestImportScriptContent, self).setUp()

        for context in self.contexts:
            with self.marionette.using_context(context):
                self.marionette.clear_imported_scripts()
        self.reset_context()

    def reset_context(self):
        self.marionette.set_context("content")

    @property
    def current_context(self):
        return self.marionette._send_message("getContext", key="value")

    @property
    def other_context(self):
        return self.contexts.copy().difference([self.current_context]).pop()

    def is_defined(self, symbol):
        return self.marionette.execute_script(
            "return typeof {} != 'undefined'".format(symbol))

    def assert_defined(self, symbol, msg=None):
        if msg is None:
            msg = "Expected symbol {} to be defined".format(symbol)
        self.assertTrue(self.is_defined(symbol), msg)

    def assert_undefined(self, symbol, msg=None):
        if msg is None:
            msg = "Expected symbol {} to be undefined".format(symbol)
        self.assertFalse(self.is_defined(symbol), msg)

    def assert_scripts_cleared(self):
        self.marionette.import_script(self.script_file)
        self.assert_defined("testFunc")
        self.marionette.clear_imported_scripts()
        self.assert_undefined("testFunc")

    def test_import_script(self):
        self.marionette.import_script(self.script_file)
        self.assertEqual(
            "i'm a test function!", self.marionette.execute_script("return testFunc();"))
        self.assertEqual("i'm a test function!", self.marionette.execute_async_script(
            "marionetteScriptFinished(testFunc());"))

    def test_import_script_twice(self):
        self.marionette.import_script(self.script_file)
        self.assert_defined("testFunc")

        # TODO(ato): Note that the WebDriver command primitives
        # does not allow us to check what scripts have been imported.
        # I suspect we must to do this through an xpcshell test.

        self.marionette.import_script(self.script_file)
        self.assert_defined("testFunc")

    def test_import_script_and_clear(self):
        self.marionette.import_script(self.script_file)
        self.assert_defined("testFunc")
        self.marionette.clear_imported_scripts()
        self.assert_scripts_cleared()
        self.assert_undefined("testFunc")
        with self.assertRaises(JavascriptException):
            self.marionette.execute_script("return testFunc()")
        with self.assertRaises(JavascriptException):
            self.marionette.execute_async_script(
                "marionetteScriptFinished(testFunc())")

    def test_clear_scripts_in_other_context(self):
        self.marionette.import_script(self.script_file)
        self.assert_defined("testFunc")

        # clearing other context's script file should not affect ours
        with self.marionette.using_context(self.other_context):
            self.marionette.clear_imported_scripts()
            self.assert_undefined("testFunc")

        self.assert_defined("testFunc")

    def test_multiple_imports(self):
        self.marionette.import_script(self.script_file)
        self.marionette.import_script(self.another_script_file)
        self.assert_defined("testFunc")
        self.assert_defined("testAnotherFunc")

    @skip_if_chrome
    def test_imports_apply_globally(self):
        self.marionette.navigate(
            self.marionette.absolute_url("test_windows.html"))

        def open_window_with_link():
            self.marionette.find_element(By.LINK_TEXT, "Open new window").click()

        new_window = self.open_window(trigger=open_window_with_link)
        self.marionette.switch_to_window(new_window)

        self.marionette.import_script(self.script_file)
        self.marionette.close_chrome_window()

        self.marionette.switch_to_window(self.start_window)
        self.assert_defined("testFunc")


class TestImportScriptChrome(TestImportScriptContent):
    def reset_context(self):
        self.marionette.set_context("chrome")
