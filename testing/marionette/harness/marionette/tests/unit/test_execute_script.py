# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import urllib
import os

from marionette_driver import By, errors
from marionette import MarionetteTestCase


def inline(doc):
    return "data:text/html;charset=utf-8,%s" % urllib.quote(doc)


elements = inline("<p>foo</p> <p>bar</p>")

globals = set([
              "atob",
              "Audio",
              "btoa",
              "document",
              "navigator",
              "URL",
              "window",
              ])


class TestExecuteSimpleTestContent(MarionetteTestCase):
    def test_stack_trace(self):
        try:
            self.marionette.execute_js_script("""
                let a = 1;
                throwHere();
                """, filename="file.js")
            self.assertFalse(True)
        except errors.JavascriptException as e:
            self.assertIn("throwHere is not defined", e.msg)
            self.assertIn("@file.js:2", e.stacktrace)


class TestExecuteContent(MarionetteTestCase):
    def test_return_number(self):
        self.assertEqual(1, self.marionette.execute_script("return 1"))
        self.assertEqual(1.5, self.marionette.execute_script("return 1.5"))

    def test_return_boolean(self):
        self.assertEqual(True, self.marionette.execute_script("return true"))

    def test_return_string(self):
        self.assertEqual("foo", self.marionette.execute_script("return 'foo'"))

    def test_return_array(self):
        self.assertEqual(
            [1, 2], self.marionette.execute_script("return [1, 2]"))
        self.assertEqual(
            [1.25, 1.75], self.marionette.execute_script("return [1.25, 1.75]"))
        self.assertEqual(
            [True, False], self.marionette.execute_script("return [true, false]"))
        self.assertEqual(
            ["foo", "bar"], self.marionette.execute_script("return ['foo', 'bar']"))
        self.assertEqual(
            [1, 1.5, True, "foo"], self.marionette.execute_script("return [1, 1.5, true, 'foo']"))
        self.assertEqual(
            [1, [2]], self.marionette.execute_script("return [1, [2]]"))

    def test_return_object(self):
        self.assertEqual(
            {"foo": 1}, self.marionette.execute_script("return {foo: 1}"))
        self.assertEqual(
            {"foo": 1.5}, self.marionette.execute_script("return {foo: 1.5}"))
        self.assertEqual(
            {"foo": True}, self.marionette.execute_script("return {foo: true}"))
        self.assertEqual(
            {"foo": "bar"}, self.marionette.execute_script("return {foo: 'bar'}"))
        self.assertEqual(
            {"foo": [1, 2]}, self.marionette.execute_script("return {foo: [1, 2]}"))
        self.assertEqual(
            {"foo": {"bar": [1, 2]}}, self.marionette.execute_script("return {foo: {bar: [1, 2]}}"))

    def test_no_return_value(self):
        self.assertIsNone(self.marionette.execute_script("true"))

    def test_argument_null(self):
        self.assertEqual(
            None, self.marionette.execute_script("return arguments[0]", [None]))

    def test_argument_number(self):
        self.assertEqual(
            1, self.marionette.execute_script("return arguments[0]", [1]))
        self.assertEqual(
            1.5, self.marionette.execute_script("return arguments[0]", [1.5]))

    def test_argument_boolean(self):
        self.assertEqual(
            True, self.marionette.execute_script("return arguments[0]", [True]))

    def test_argument_string(self):
        self.assertEqual(
            "foo", self.marionette.execute_script("return arguments[0]", ["foo"]))

    def test_argument_array(self):
        self.assertEqual(
            [1, 2], self.marionette.execute_script("return arguments[0]", [[1, 2]]))

    def test_argument_object(self):
        self.assertEqual({"foo": 1}, self.marionette.execute_script(
            "return arguments[0]", [{"foo": 1}]))

    def assert_is_defined(self, property, sandbox="default"):
        self.assertTrue(self.marionette.execute_script(
            "return typeof %s != 'undefined'" % property,
            sandbox=sandbox),
            "property %s is undefined" % property)

    def test_globals(self):
        for property in globals:
            self.assert_is_defined(property)
        self.assert_is_defined("Components")
        self.assert_is_defined("window.wrappedJSObject")

    def test_system_globals(self):
        for property in globals:
            self.assert_is_defined(property, sandbox="system")
        self.assert_is_defined("Components", sandbox="system")
        self.assert_is_defined("window.wrappedJSObject")

    def test_exception(self):
        self.assertRaises(errors.JavascriptException,
                          self.marionette.execute_script, "return foo")

    def test_stacktrace(self):
        try:
            self.marionette.execute_script("return b")
            self.assertFalse(True)
        except errors.JavascriptException as e:
            # by default execute_script pass the name of the python file
            self.assertIn(
                os.path.basename(__file__.replace(".pyc", ".py")), e.stacktrace)
            self.assertIn("b is not defined", e.msg)
            self.assertIn("return b", e.stacktrace)

    def test_permission(self):
        with self.assertRaises(errors.JavascriptException):
            self.marionette.execute_script("""
                let prefs = Components.classes["@mozilla.org/preferences-service;1"]
                    .getService(Components.interfaces.nsIPrefBranch)""")

    def test_return_web_element(self):
        self.marionette.navigate(elements)
        expected = self.marionette.find_element(By.TAG_NAME, "p")
        actual = self.marionette.execute_script(
            "return document.querySelector('p')")
        self.assertEqual(expected, actual)

    def test_return_web_element_array(self):
        self.marionette.navigate(elements)
        expected = self.marionette.find_elements(By.TAG_NAME, "p")
        actual = self.marionette.execute_script("""
            let els = document.querySelectorAll('p')
            return [els[0], els[1]]""")
        self.assertEqual(expected, actual)

    # Bug 938228 identifies a problem with unmarshaling NodeList
    # objects from the DOM.  document.querySelectorAll returns this
    # construct.
    def test_return_web_element_nodelist(self):
        self.marionette.navigate(elements)
        expected = self.marionette.find_elements(By.TAG_NAME, "p")
        actual = self.marionette.execute_script(
            "return document.querySelectorAll('p')")
        self.assertEqual(expected, actual)

    def test_sandbox_reuse(self):
        # Sandboxes between `execute_script()` invocations are shared.
        self.marionette.execute_script("this.foobar = [23, 42];")
        self.assertEqual(self.marionette.execute_script(
            "return this.foobar;", new_sandbox=False), [23, 42])

        self.marionette.execute_script("global.barfoo = [42, 23];")
        self.assertEqual(self.marionette.execute_script(
            "return global.barfoo;", new_sandbox=False), [42, 23])

    def test_sandbox_refresh_arguments(self):
        self.marionette.execute_script(
            "this.foobar = [arguments[0], arguments[1]]", [23, 42])
        self.assertEqual(self.marionette.execute_script(
            "return this.foobar", new_sandbox=False), [23, 42])

    def test_wrappedjsobject(self):
        self.marionette.execute_script("window.wrappedJSObject.foo = 3")
        self.assertEqual(
            3, self.marionette.execute_script("return window.wrappedJSObject.foo"))

    def test_system_sandbox_wrappedjsobject(self):
        self.marionette.execute_script(
            "window.wrappedJSObject.foo = 4", sandbox="system")
        self.assertEqual(4, self.marionette.execute_script(
            "return window.wrappedJSObject.foo", sandbox="system"))

    def test_system_dead_object(self):
        self.marionette.execute_script(
            "window.wrappedJSObject.foo = function() { return 'yo' }",
            sandbox="system")
        self.marionette.execute_script(
            "dump(window.wrappedJSObject.foo)", sandbox="system")

        self.marionette.execute_script(
            "window.wrappedJSObject.foo = function() { return 'yolo' }",
            sandbox="system")
        typ = self.marionette.execute_script(
            "return typeof window.wrappedJSObject.foo", sandbox="system")
        self.assertEqual("function", typ)
        obj = self.marionette.execute_script(
            "return window.wrappedJSObject.foo.toString()", sandbox="system")
        self.assertIn("yolo", obj)

    def test_lasting_side_effects(self):
        def send(script):
            return self.marionette._send_message(
                "executeScript", {"script": script}, key="value")

        send("window.foo = 1")
        foo = send("return window.foo")
        self.assertEqual(1, foo)

        for property in globals:
            exists = send("return typeof %s != 'undefined'" % property)
            self.assertTrue(exists, "property %s is undefined" % property)
        # TODO(ato): For some reason this fails, probably Sandbox bug?
        # self.assertTrue(send("return typeof Components == 'undefined'"))
        self.assertTrue(
            send("return typeof window.wrappedJSObject == 'undefined'"))


class TestExecuteChrome(TestExecuteContent):
    def setUp(self):
        TestExecuteContent.setUp(self)
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

    def test_permission(self):
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

    def test_invalid_chrome_handle(self):
        # Close second chrome window and don't switch back to the original one
        self.marionette.close_chrome_window()
        self.assertEqual(len(self.marionette.chrome_window_handles), 1)

        # Call execute_script on an invalid chrome handle
        with self.marionette.using_context('chrome'):
            self.marionette.execute_script("""
                return true;
            """)

    def test_lasting_side_effects(self):
        pass

    def test_return_web_element(self):
        pass

    def test_return_web_element_array(self):
        pass

    def test_return_web_element_nodelist(self):
        pass
