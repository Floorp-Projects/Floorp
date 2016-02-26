# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import urllib
import os

from marionette_driver import By, errors
from marionette.marionette_test import MarionetteTestCase, skip_if_b2g


def inline(doc):
    return "data:text/html;charset=utf-8,%s" % urllib.quote(doc)


elements = inline("<p>foo</p> <p>bar</p>")


class TestExecuteContent(MarionetteTestCase):
    def test_execute_js_script_stack_trace(self):
        try:
            self.marionette.execute_js_script("""
                let a = 1;
                throwHere();
                """, filename="file.js")
            self.assertFalse(True)
        except errors.JavascriptException as inst:
            self.assertIn('throwHere is not defined', inst.msg)
            self.assertIn('@file.js:2', inst.stacktrace)

    def test_execute_script_stack_trace(self):
        try:
            self.marionette.execute_script("""
                let a = 1;
                return b;
                """)
            self.assertFalse(True)
        except errors.JavascriptException as inst:
            # By default execute_script pass the name of the python file
            self.assertIn(os.path.basename(__file__.replace(".pyc", ".py")), inst.stacktrace)
            self.assertIn('b is not defined', inst.msg)
            self.assertIn('return b', inst.stacktrace)

    def test_execute_simple(self):
        self.assertEqual(1, self.marionette.execute_script("return 1;"))

    def test_check_window(self):
        self.assertTrue(self.marionette.execute_script("return (window !=null && window != undefined);"))

    def test_execute_no_return(self):
        self.assertEqual(self.marionette.execute_script("1;"), None)

    def test_execute_js_exception(self):
        self.assertRaises(errors.JavascriptException,
            self.marionette.execute_script, "return foo(bar);")

    def test_execute_permission(self):
        self.assertRaises(errors.JavascriptException,
                          self.marionette.execute_script,
                          """
let prefs = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefBranch);
""")

    def test_complex_return_values(self):
        self.assertEqual(self.marionette.execute_script("return [1, 2];"), [1, 2])
        self.assertEqual(self.marionette.execute_script("return {'foo': 'bar', 'fizz': 'fazz'};"),
                         {'foo': 'bar', 'fizz': 'fazz'})
        self.assertEqual(self.marionette.execute_script("return [1, {'foo': 'bar'}, 2];"),
                         [1, {'foo': 'bar'}, 2])
        self.assertEqual(self.marionette.execute_script("return {'foo': [1, 'a', 2]};"),
                         {'foo': [1, 'a', 2]})

    # Bug 938228 identifies a problem with unmarshaling NodeList
    # objects from the DOM.  document.querySelectorAll returns this
    # construct.
    def test_unmarshal_element_collection(self):
        self.marionette.navigate(elements)
        expected = self.marionette.find_elements(By.TAG_NAME, "p")
        actual = self.marionette.execute_script(
            "return document.querySelectorAll('p')")
        self.assertEqual(expected, actual)

    def test_sandbox_reuse(self):
        # Sandboxes between `execute_script()` invocations are shared.
        self.marionette.execute_script("this.foobar = [23, 42];")
        self.assertEqual(self.marionette.execute_script("return this.foobar;", new_sandbox=False), [23, 42])

        self.marionette.execute_script("global.barfoo = [42, 23];")
        self.assertEqual(self.marionette.execute_script("return global.barfoo;", new_sandbox=False), [42, 23])

    def test_sandbox_refresh_arguments(self):
        self.marionette.execute_script("this.foobar = [arguments[0], arguments[1]];",
                                       script_args=[23, 42])
        self.assertEqual(self.marionette.execute_script("return this.foobar;", new_sandbox=False),
                         [23, 42])

        self.marionette.execute_script("global.barfoo = [arguments[0], arguments[1]];",
                                       script_args=[42, 23],
                                       new_sandbox=False)
        self.assertEqual(self.marionette.execute_script("return global.barfoo;", new_sandbox=False),
                         [42, 23])

    def test_that_we_can_pass_in_floats(self):
        expected_result = 1.2
        result = self.marionette.execute_script("return arguments[0]",
                                                [expected_result])
        self.assertTrue(isinstance(result, float))
        self.assertEqual(result, expected_result)

    def test_null_argument(self):
        result = self.marionette.execute_script("return arguments[0]",
                                                [None])
        self.assertIs(result, None)


@skip_if_b2g
class TestExecuteChrome(TestExecuteContent):
    def setUp(self):
        super(TestExecuteChrome, self).setUp()
        self.win = self.marionette.current_window_handle
        self.marionette.set_context("chrome")
        self.marionette.execute_script(
            "window.open('chrome://marionette/content/test.xul', 'xul', 'chrome')")
        self.marionette.switch_to_window("xul")
        self.assertNotEqual(self.win, self.marionette.current_window_handle)

    def tearDown(self):
        self.marionette.execute_script("window.close()")
        self.marionette.switch_to_window(self.win)
        self.assertEqual(self.win, self.marionette.current_window_handle)
        super(TestExecuteChrome, self).tearDown()

    def test_execute_permission(self):
        self.assertEqual(1, self.marionette.execute_script(
            "var c = Components.classes; return 1;"))

    def test_unmarshal_element_collection(self):
        expected = self.marionette.find_elements(By.TAG_NAME, "textbox")
        actual = self.marionette.execute_script(
            "return document.querySelectorAll('textbox')")
        self.assertEqual(expected, actual)

    def test_async_script_timeout(self):
        with self.assertRaises(errors.ScriptTimeoutException):
            self.marionette.execute_async_script("""
                var cb = arguments[arguments.length - 1];
                setTimeout(function() { cb() }, 250);
                """, script_timeout=100)
