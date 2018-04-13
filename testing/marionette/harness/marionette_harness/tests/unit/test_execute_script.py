from __future__ import absolute_import

import os
import urllib

from marionette_driver import By, errors
from marionette_driver.marionette import Alert, HTMLElement
from marionette_driver.wait import Wait

from marionette_harness import MarionetteTestCase, skip_if_mobile, WindowManagerMixin


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


class TestExecuteContent(MarionetteTestCase):

    def alert_present(self):
        try:
            Alert(self.marionette).text
            return True
        except errors.NoAlertPresentException:
            return False

    def wait_for_alert_closed(self, timeout=None):
        Wait(self.marionette, timeout=timeout).until(
            lambda _: not self.alert_present())

    def tearDown(self):
        if self.alert_present():
            alert = self.marionette.switch_to_alert()
            alert.dismiss()
            self.wait_for_alert_closed()

    def assert_is_defined(self, property, sandbox="default"):
        self.assertTrue(self.marionette.execute_script(
            "return typeof arguments[0] != 'undefined'", [property], sandbox=sandbox),
            "property {} is undefined".format(property))

    def assert_is_web_element(self, element):
        self.assertIsInstance(element, HTMLElement)

    def test_return_number(self):
        self.assertEqual(1, self.marionette.execute_script("return 1"))
        self.assertEqual(1.5, self.marionette.execute_script("return 1.5"))

    def test_return_boolean(self):
        self.assertTrue(self.marionette.execute_script("return true"))

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
            {"foo": {"bar": [1, 2]}},
            self.marionette.execute_script("return {foo: {bar: [1, 2]}}"))

    def test_no_return_value(self):
        self.assertIsNone(self.marionette.execute_script("true"))

    def test_argument_null(self):
        self.assertIsNone(self.marionette.execute_script(
            "return arguments[0]",
            script_args=(None,),
            sandbox="default"))
        self.assertIsNone(self.marionette.execute_script(
            "return arguments[0]",
            script_args=(None,),
            sandbox="system"))
        self.assertIsNone(self.marionette.execute_script(
            "return arguments[0]",
            script_args=(None,),
            sandbox=None))

    def test_argument_number(self):
        self.assertEqual(
            1, self.marionette.execute_script("return arguments[0]", (1,)))
        self.assertEqual(
            1.5, self.marionette.execute_script("return arguments[0]", (1.5,)))

    def test_argument_boolean(self):
        self.assertTrue(self.marionette.execute_script("return arguments[0]", (True,)))

    def test_argument_string(self):
        self.assertEqual(
            "foo", self.marionette.execute_script("return arguments[0]", ("foo",)))

    def test_argument_array(self):
        self.assertEqual(
            [1, 2], self.marionette.execute_script("return arguments[0]", ([1, 2],)))

    def test_argument_object(self):
        self.assertEqual({"foo": 1}, self.marionette.execute_script(
            "return arguments[0]", ({"foo": 1},)))

    def test_default_sandbox_globals(self):
        for property in globals:
            self.assert_is_defined(property, sandbox="default")

        self.assert_is_defined("Components")
        self.assert_is_defined("window.wrappedJSObject")

    def test_system_globals(self):
        for property in globals:
            self.assert_is_defined(property, sandbox="system")

        self.assert_is_defined("Components", sandbox="system")
        self.assert_is_defined("window.wrappedJSObject", sandbox="system")

    def test_mutable_sandbox_globals(self):
        for property in globals:
            self.assert_is_defined(property, sandbox=None)

        # Components is there, but will be removed soon
        self.assert_is_defined("Components", sandbox=None)
        # wrappedJSObject is always there in sandboxes
        self.assert_is_defined("window.wrappedJSObject", sandbox=None)

    def test_exception(self):
        self.assertRaises(errors.JavascriptException,
                          self.marionette.execute_script, "return foo")

    def test_stacktrace(self):
        with self.assertRaises(errors.JavascriptException) as cm:
            self.marionette.execute_script("return b")

        # by default execute_script pass the name of the python file
        self.assertIn(os.path.relpath(__file__.replace(".pyc", ".py")), cm.exception.stacktrace)
        self.assertIn("b is not defined", cm.exception.message)

    def test_permission(self):
        for sandbox in ["default", None]:
            with self.assertRaises(errors.JavascriptException):
               self.marionette.execute_script(
                    "Components.classes['@mozilla.org/preferences-service;1']")

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

    def test_sandbox_refresh_arguments(self):
        self.marionette.execute_script(
            "this.foobar = [arguments[0], arguments[1]]", [23, 42])
        self.assertEqual(self.marionette.execute_script(
            "return this.foobar", new_sandbox=False), [23, 42])

    def test_mutable_sandbox_wrappedjsobject(self):
        self.assert_is_defined("window.wrappedJSObject")
        with self.assertRaises(errors.JavascriptException):
            self.marionette.execute_script("window.wrappedJSObject.foo = 1", sandbox=None)

    def test_default_sandbox_wrappedjsobject(self):
        self.assert_is_defined("window.wrappedJSObject", sandbox="default")

        try:
            self.marionette.execute_script(
                "window.wrappedJSObject.foo = 4", sandbox="default")
            self.assertEqual(self.marionette.execute_script(
                "return window.wrappedJSObject.foo", sandbox="default"), 4)
        finally:
            self.marionette.execute_script(
                "delete window.wrappedJSObject.foo", sandbox="default")

    def test_system_sandbox_wrappedjsobject(self):
        self.assert_is_defined("window.wrappedJSObject", sandbox="system")

        self.marionette.execute_script(
            "window.wrappedJSObject.foo = 4", sandbox="system")
        self.assertEqual(self.marionette.execute_script(
            "return window.wrappedJSObject.foo", sandbox="system"), 4)

    def test_system_dead_object(self):
        self.assert_is_defined("window.wrappedJSObject", sandbox="system")

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

        self.assertTrue(send("return (typeof Components == 'undefined') || (typeof Components.utils == 'undefined')"))
        self.assertTrue(send("return typeof window.wrappedJSObject == 'undefined'"))

    def test_no_callback(self):
        self.assertTrue(self.marionette.execute_script(
            "return typeof arguments[0] == 'undefined'"))

    @skip_if_mobile("Intermittent on Android - bug 1334035")
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

    def test_access_chrome_objects_in_event_listeners(self):
        # sandbox.window.addEventListener/removeEventListener
        # is used by Marionette for installing the unloadHandler which
        # is used to return an error when a document is unloaded during
        # script execution.
        #
        # Certain web frameworks, notably Angular, override
        # window.addEventListener/removeEventListener and introspects
        # objects passed to them.  If these objects originates from chrome
        # without having been cloned, a permission denied error is thrown
        # as part of the security precautions put in place by the sandbox.

        # addEventListener is called when script is injected
        self.marionette.navigate(inline("""
            <script>
            window.addEventListener = (event, listener) => listener.toString();
            </script>
            """))
        self.marionette.execute_script("", sandbox=None)

        # removeEventListener is called when sandbox is unloaded
        self.marionette.navigate(inline("""
            <script>
            window.removeEventListener = (event, listener) => listener.toString();
            </script>
            """))
        self.marionette.execute_script("", sandbox=None)

    def test_access_global_objects_from_chrome(self):
        # test inspection of arguments
        self.marionette.execute_script("__webDriverArguments.toString()")

    def test_toJSON(self):
        foo = self.marionette.execute_script("""
            return {
              toJSON () {
                return "foo";
              }
            }""",
            sandbox=None)
        self.assertEqual("foo", foo)

    def test_unsafe_toJSON(self):
        el = self.marionette.execute_script("""
            return {
              toJSON () {
                return document.documentElement;
              }
            }""",
            sandbox=None)
        self.assert_is_web_element(el)

    @skip_if_mobile("Modal dialogs not supported in Fennec")
    def test_return_value_on_alert(self):
        res = self.marionette._send_message("executeScript", {"script": "alert()"})
        self.assertIn("value", res)
        self.assertIsNone(res["value"])


class TestExecuteChrome(WindowManagerMixin, TestExecuteContent):

    def setUp(self):
        super(TestExecuteChrome, self).setUp()

        self.marionette.set_context("chrome")

    def tearDown(self):
        super(TestExecuteChrome, self).tearDown()

    def test_permission(self):
        self.marionette.execute_script(
            "Components.classes['@mozilla.org/preferences-service;1']")

    @skip_if_mobile("New windows not supported in Fennec")
    def test_unmarshal_element_collection(self):

        def open_window_with_js():
            self.marionette.execute_script(
                "window.open('chrome://marionette/content/test.xul', 'xul', 'chrome');")

        try:
            win = self.open_window(trigger=open_window_with_js)
            self.marionette.switch_to_window(win)

            expected = self.marionette.find_elements(By.TAG_NAME, "textbox")
            actual = self.marionette.execute_script(
                "return document.querySelectorAll('textbox')")
            self.assertEqual(expected, actual)

        finally:
            self.close_all_windows()

    def test_async_script_timeout(self):
        with self.assertRaises(errors.ScriptTimeoutException):
            self.marionette.execute_async_script("""
                var cb = arguments[arguments.length - 1];
                setTimeout(function() { cb() }, 2500);
                """, script_timeout=100)

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

    def test_mutable_sandbox_wrappedjsobject(self):
        pass

    def test_default_sandbox_wrappedjsobject(self):
        pass

    def test_system_sandbox_wrappedjsobject(self):
        pass

    def test_access_chrome_objects_in_event_listeners(self):
        pass

    def test_return_value_on_alert(self):
        pass
