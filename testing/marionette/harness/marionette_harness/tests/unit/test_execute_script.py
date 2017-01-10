# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import urllib

from marionette_driver import By, errors
from marionette_driver.marionette import HTMLElement
from marionette_driver.wait import Wait

from marionette_harness import MarionetteTestCase, WindowManagerMixin


def inline(doc):
    return "data:text/html;charset=utf-8,{}".format(urllib.quote(doc))


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
            self.assertIn("throwHere is not defined", e.message)
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
            "return typeof {} != 'undefined'".format(property),
            sandbox=sandbox),
                        "property {} is undefined".format(property))

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
            self.assertIn("b is not defined", e.message)
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
            exists = send("return typeof {} != 'undefined'".format(property))
            self.assertTrue(exists, "property {} is undefined".format(property))
        # TODO(ato): For some reason this fails, probably Sandbox bug?
        # self.assertTrue(send("return typeof Components == 'undefined'"))
        self.assertTrue(
            send("return typeof window.wrappedJSObject == 'undefined'"))

    def test_no_callback(self):
        self.assertTrue(self.marionette.execute_script(
            "return typeof arguments[0] == 'undefined'"))

    def test_window_set_timeout_is_not_cancelled(self):
        def content_timeout_triggered(mn):
            return mn.execute_script("return window.n", sandbox=None) > 0

        # subsequent call to execute_script after this
        # should not cancel the setTimeout event
        self.marionette.navigate(inline("""
            <script>
            window.n = 0;
            setTimeout(() => ++window.n, 4000);
            </script>"""))

        # as debug builds are inherently slow,
        # we need to assert the event did not already fire
        self.assertEqual(0, self.marionette.execute_script(
            "return window.n", sandbox=None),
            "setTimeout already fired")

        # if event was cancelled, this will time out
        Wait(self.marionette, timeout=8).until(
            content_timeout_triggered,
            message="Scheduled setTimeout event was cancelled by call to execute_script")


class TestExecuteChrome(WindowManagerMixin, TestExecuteContent):

    def setUp(self):
        super(TestExecuteChrome, self).setUp()

        self.marionette.set_context("chrome")

        def open_window_with_js():
            self.marionette.execute_script(
                "window.open('chrome://marionette/content/test.xul', 'xul', 'chrome');")

        new_window = self.open_window(trigger=open_window_with_js)
        self.marionette.switch_to_window(new_window)

    def tearDown(self):
        self.close_all_windows()
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

    def test_window_set_timeout_is_not_cancelled(self):
        pass


class TestElementCollections(MarionetteTestCase):
    def assertSequenceIsInstance(self, seq, typ):
        for item in seq:
            self.assertIsInstance(item, typ)

    def test_array(self):
        self.marionette.navigate(inline("<p>foo <p>bar"))
        els = self.marionette.execute_script("return Array.from(document.querySelectorAll('p'))")
        self.assertIsInstance(els, list)
        self.assertEqual(2, len(els))
        self.assertSequenceIsInstance(els, HTMLElement)

    def test_html_all_collection(self):
        self.marionette.navigate(inline("<p>foo <p>bar"))
        els = self.marionette.execute_script("return document.all")
        self.assertIsInstance(els, list)
        # <html>, <head>, <body>, <p>, <p>
        self.assertEqual(5, len(els))
        self.assertSequenceIsInstance(els, HTMLElement)

    def test_html_collection(self):
        self.marionette.navigate(inline("<p>foo <p>bar"))
        els = self.marionette.execute_script("return document.getElementsByTagName('p')")
        self.assertIsInstance(els, list)
        self.assertEqual(2, len(els))
        self.assertSequenceIsInstance(els, HTMLElement)

    def test_html_form_controls_collection(self):
        self.marionette.navigate(inline("<form><input><input></form>"))
        els = self.marionette.execute_script("return document.forms[0].elements")
        self.assertIsInstance(els, list)
        self.assertEqual(2, len(els))
        self.assertSequenceIsInstance(els, HTMLElement)

    def test_html_options_collection(self):
        self.marionette.navigate(inline("<select><option><option></select>"))
        els = self.marionette.execute_script("return document.querySelector('select').options")
        self.assertIsInstance(els, list)
        self.assertEqual(2, len(els))
        self.assertSequenceIsInstance(els, HTMLElement)

    def test_node_list(self):
        self.marionette.navigate(inline("<p>foo <p>bar"))
        els = self.marionette.execute_script("return document.querySelectorAll('p')")
        self.assertIsInstance(els, list)
        self.assertEqual(2, len(els))
        self.assertSequenceIsInstance(els, HTMLElement)
