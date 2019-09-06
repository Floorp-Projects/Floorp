# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_driver.errors import JavascriptException

from marionette_harness import MarionetteTestCase


class TestExecuteSandboxes(MarionetteTestCase):
    def setUp(self):
        super(TestExecuteSandboxes, self).setUp()

    def test_execute_system_sandbox(self):
        # Test that "system" sandbox has elevated privileges in execute_script
        result = self.marionette.execute_script(
            "return Components.interfaces.nsIPermissionManager.ALLOW_ACTION",
            sandbox="system")
        self.assertEqual(result, 1)

    def test_execute_async_system_sandbox(self):
        # Test that "system" sandbox has elevated privileges in
        # execute_async_script.
        result = self.marionette.execute_async_script("""
            let result = Ci.nsIPermissionManager.ALLOW_ACTION;
            arguments[0](result);""",
            sandbox="system")
        self.assertEqual(result, 1)

    def test_execute_switch_sandboxes(self):
        # Test that sandboxes are retained when switching between them
        # for execute_script.
        self.marionette.execute_script("foo = 1", sandbox="1")
        self.marionette.execute_script("foo = 2", sandbox="2")
        foo = self.marionette.execute_script(
            "return foo", sandbox="1", new_sandbox=False)
        self.assertEqual(foo, 1)
        foo = self.marionette.execute_script(
            "return foo", sandbox="2", new_sandbox=False)
        self.assertEqual(foo, 2)

    def test_execute_new_sandbox(self):
        # test that clearing a sandbox does not affect other sandboxes
        self.marionette.execute_script("foo = 1", sandbox="1")
        self.marionette.execute_script("foo = 2", sandbox="2")

        # deprecate sandbox 1 by asking explicitly for a fresh one
        with self.assertRaises(JavascriptException):
            self.marionette.execute_script("return foo",
                sandbox="1", new_sandbox=True)

        foo = self.marionette.execute_script(
            "return foo", sandbox="2", new_sandbox=False)
        self.assertEqual(foo, 2)

    def test_execute_async_switch_sandboxes(self):
        # Test that sandboxes are retained when switching between them
        # for execute_async_script.
        self.marionette.execute_async_script(
            "foo = 1; arguments[0]();", sandbox="1")
        self.marionette.execute_async_script(
            "foo = 2; arguments[0]();", sandbox='2')
        foo = self.marionette.execute_async_script(
            "arguments[0](foo);",
            sandbox="1",
            new_sandbox=False)
        self.assertEqual(foo, 1)
        foo = self.marionette.execute_async_script(
            "arguments[0](foo);",
            sandbox="2",
            new_sandbox=False)
        self.assertEqual(foo, 2)


class TestExecuteSandboxesChrome(TestExecuteSandboxes):
    def setUp(self):
        super(TestExecuteSandboxesChrome, self).setUp()
        self.marionette.set_context("chrome")
