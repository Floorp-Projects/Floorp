# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_test import MarionetteTestCase, skip_if_b2g
from errors import JavascriptException, MarionetteException, ScriptTimeoutException

class TestExecuteAsyncContent(MarionetteTestCase):
    def setUp(self):
        super(TestExecuteAsyncContent, self).setUp()
        self.marionette.set_script_timeout(1000)

    def test_execute_async_simple(self):
        self.assertEqual(1, self.marionette.execute_async_script("arguments[arguments.length-1](1);"))

    def test_execute_async_ours(self):
        self.assertEqual(1, self.marionette.execute_async_script("marionetteScriptFinished(1);"))

    def test_execute_async_timeout(self):
        self.assertRaises(ScriptTimeoutException, self.marionette.execute_async_script, "var x = 1;")

    def test_no_timeout(self):
        self.marionette.set_script_timeout(2000)
        self.assertTrue(self.marionette.execute_async_script("""
            var callback = arguments[arguments.length - 1];
            setTimeout(function() { callback(true); }, 500);
            """))

    @skip_if_b2g
    def test_execute_async_unload(self):
        self.marionette.set_script_timeout(5000)
        unload = """
                window.location.href = "about:blank";
                 """
        self.assertRaises(JavascriptException, self.marionette.execute_async_script, unload)

    def test_check_window(self):
        self.assertTrue(self.marionette.execute_async_script("marionetteScriptFinished(window !=null && window != undefined);"))

    def test_same_context(self):
        var1 = 'testing'
        self.assertEqual(self.marionette.execute_script("""
            this.testvar = '%s';
            return this.testvar;
            """ % var1), var1)
        self.assertEqual(self.marionette.execute_async_script(
            "marionetteScriptFinished(this.testvar);", new_sandbox=False), var1)

    def test_execute_no_return(self):
        self.assertEqual(self.marionette.execute_async_script("marionetteScriptFinished()"), None)

    def test_execute_js_exception(self):
        self.assertRaises(JavascriptException,
            self.marionette.execute_async_script, "foo(bar);")

    def test_execute_async_js_exception(self):
        self.assertRaises(JavascriptException,
            self.marionette.execute_async_script, """
            var callback = arguments[arguments.length - 1];
            callback(foo());
            """)

    def test_script_finished(self):
        self.assertTrue(self.marionette.execute_async_script("""
            marionetteScriptFinished(true);
            """))

    def test_execute_permission(self):
        self.assertRaises(JavascriptException, self.marionette.execute_async_script, """
let prefs = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefBranch);
marionetteScriptFinished(1);
""")

    def test_sandbox_reuse(self):
        # Sandboxes between `execute_script()` invocations are shared.
        self.marionette.execute_async_script("this.foobar = [23, 42];"
                                             "marionetteScriptFinished();")
        self.assertEqual(self.marionette.execute_async_script(
            "marionetteScriptFinished(this.foobar);", new_sandbox=False), [23, 42])

        self.marionette.execute_async_script("global.barfoo = [42, 23];"
                                             "marionetteScriptFinished();")
        self.assertEqual(self.marionette.execute_async_script(
            "marionetteScriptFinished(global.barfoo);", new_sandbox=False), [42, 23])


class TestExecuteAsyncChrome(TestExecuteAsyncContent):
    def setUp(self):
        super(TestExecuteAsyncChrome, self).setUp()
        self.marionette.set_context("chrome")

    def test_execute_async_unload(self):
        pass

    def test_same_context(self):
        pass

    def test_execute_permission(self):
        self.assertEqual(1, self.marionette.execute_async_script("""
var c = Components.classes;
marionetteScriptFinished(1);
"""))

    def test_sandbox_reuse(self):
        pass

