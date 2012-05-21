# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_test import MarionetteTestCase
from errors import JavascriptException, MarionetteException

class TestExecuteContent(MarionetteTestCase):
    def test_execute_simple(self):
        self.assertEqual(1, self.marionette.execute_script("return 1;"))

    def test_check_window(self):
        self.assertTrue(self.marionette.execute_script("return (window !=null && window != undefined);"))

    def test_execute_no_return(self):
        self.assertEqual(self.marionette.execute_script("1;"), None)

    def test_execute_js_exception(self):
        self.assertRaises(JavascriptException,
            self.marionette.execute_script, "return foo(bar);")

    def test_execute_permission(self):
        self.assertRaises(JavascriptException,
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

    def test_sandbox_reuse(self):
        # Sandboxes between `execute_script()` invocations are shared.
        self.marionette.execute_script("this.foobar = [23, 42];")
        self.assertEqual(self.marionette.execute_script("return this.foobar;", new_sandbox=False), [23, 42])

        self.marionette.execute_script("global.barfoo = [42, 23];")
        self.assertEqual(self.marionette.execute_script("return global.barfoo;", new_sandbox=False), [42, 23])
    
    def test_that_we_can_pass_in_floats(self):
	expected_result = 1.2
	result = self.marionette.execute_script("return arguments[0]", 
					[expected_result])
	self.assertTrue(isinstance(result, float))
	self.assertEqual(result, expected_result)

class TestExecuteChrome(TestExecuteContent):
    def setUp(self):
        super(TestExecuteChrome, self).setUp()
        self.marionette.set_context("chrome")

    def test_execute_permission(self):
        self.assertEqual(1, self.marionette.execute_script("var c = Components.classes;return 1;"))

    def test_sandbox_reuse(self):
        pass
