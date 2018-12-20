from __future__ import absolute_import

import os

from marionette_driver.errors import (
    JavascriptException,
    ScriptTimeoutException,
)

from marionette_harness import MarionetteTestCase


class TestExecuteAsyncContent(MarionetteTestCase):
    def setUp(self):
        super(TestExecuteAsyncContent, self).setUp()
        self.marionette.timeout.script = 1

    def test_execute_async_simple(self):
        self.assertEqual(1, self.marionette.execute_async_script("arguments[arguments.length-1](1);"))

    def test_execute_async_ours(self):
        self.assertEqual(1, self.marionette.execute_async_script("arguments[0](1);"))

    def test_execute_async_timeout(self):
        self.assertRaises(ScriptTimeoutException, self.marionette.execute_async_script, "var x = 1;")

    def test_execute_async_unique_timeout(self):
        self.assertEqual(2, self.marionette.execute_async_script("setTimeout(() => arguments[0](2), 2000);", script_timeout=5000))
        self.assertRaises(ScriptTimeoutException, self.marionette.execute_async_script, "setTimeout(() => arguments[0](3), 2000);")

    def test_no_timeout(self):
        self.marionette.timeout.script = 10
        self.assertTrue(self.marionette.execute_async_script("""
            var callback = arguments[arguments.length - 1];
            setTimeout(function() { callback(true); }, 500);
            """))

    def test_execute_async_unload(self):
        self.marionette.timeout.script = 5
        unload = """
                window.location.href = "about:blank";
                 """
        self.assertRaises(JavascriptException, self.marionette.execute_async_script, unload)

    def test_check_window(self):
        self.assertTrue(self.marionette.execute_async_script("arguments[0](window != null && window != undefined);"))

    def test_same_context(self):
        var1 = 'testing'
        self.assertEqual(self.marionette.execute_script("""
            this.testvar = '{}';
            return this.testvar;
            """.format(var1)), var1)
        self.assertEqual(self.marionette.execute_async_script(
            "arguments[0](this.testvar);", new_sandbox=False), var1)

    def test_execute_no_return(self):
        self.assertEqual(self.marionette.execute_async_script("arguments[0]()"), None)

    def test_execute_js_exception(self):
        try:
            self.marionette.execute_async_script("""
                let a = 1;
                foo(bar);
                """)
            self.fail()
        except JavascriptException as e:
            self.assertIsNotNone(e.stacktrace)
            self.assertIn(os.path.relpath(__file__.replace(".pyc", ".py")), e.stacktrace)

    def test_execute_async_js_exception(self):
        try:
            self.marionette.execute_async_script("""
                let [resolve] = arguments;
                resolve(foo());
            """)
            self.fail()
        except JavascriptException as e:
            self.assertIsNotNone(e.stacktrace)
            self.assertIn(os.path.relpath(__file__.replace(".pyc", ".py")), e.stacktrace)

    def test_script_finished(self):
        self.assertTrue(self.marionette.execute_async_script("""
            arguments[0](true);
            """))

    def test_execute_permission(self):
        self.assertRaises(JavascriptException, self.marionette.execute_async_script, """
let prefs = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefBranch);
arguments[0](4);
""")

    def test_sandbox_reuse(self):
        # Sandboxes between `execute_script()` invocations are shared.
        self.marionette.execute_async_script("this.foobar = [23, 42];"
                                             "arguments[0]();")
        self.assertEqual(self.marionette.execute_async_script(
            "arguments[0](this.foobar);", new_sandbox=False), [23, 42])

    def test_sandbox_refresh_arguments(self):
        self.marionette.execute_async_script("this.foobar = [arguments[0], arguments[1]];"
                                             "let resolve = "
                                                 "arguments[arguments.length - 1];"
                                             "resolve();",
                                             script_args=[23, 42])
        self.assertEqual(self.marionette.execute_async_script(
            "arguments[0](this.foobar);", new_sandbox=False),
                         [23, 42])

    # Functions defined in higher privilege scopes, such as the privileged
    # content frame script listener.js runs in, cannot be accessed from
    # content.  This tests that it is possible to introspect the objects on
    # `arguments` without getting permission defined errors.  This is made
    # possible because the last argument is always the callback/complete
    # function.
    #
    # See bug 1290966.
    def test_introspection_of_arguments(self):
        self.marionette.execute_async_script(
            "arguments[0].cheese; __webDriverCallback();",
            script_args=[], sandbox=None)


class TestExecuteAsyncChrome(TestExecuteAsyncContent):
    def setUp(self):
        super(TestExecuteAsyncChrome, self).setUp()
        self.marionette.set_context("chrome")

    def test_execute_async_unload(self):
        pass

    def test_execute_permission(self):
        self.assertEqual(5, self.marionette.execute_async_script("""
var c = Components.classes;
arguments[0](5);
"""))

    def test_execute_async_js_exception(self):
        # Javascript exceptions are not propagated in chrome code
        self.marionette.timeout.script = 0.2
        self.assertRaises(ScriptTimeoutException,
            self.marionette.execute_async_script, """
            var callback = arguments[arguments.length - 1];
            setTimeout("callback(foo())", 50);
            """)
