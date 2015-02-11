# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_test import MarionetteTestCase, skip_if_b2g
from errors import JavascriptException, MarionetteException, ScriptTimeoutException

class TestExecuteIsolationContent(MarionetteTestCase):
    def setUp(self):
        super(TestExecuteIsolationContent, self).setUp()
        self.content = True

    def test_execute_async_isolate(self):
        # Results from one execute call that has timed out should not
        # contaminate a future call.
        multiplier = "*3" if self.content else "*1"
        self.marionette.set_script_timeout(500)
        self.assertRaises(ScriptTimeoutException,
                          self.marionette.execute_async_script,
                          ("setTimeout(function() { marionetteScriptFinished(5%s); }, 3000);"
                               % multiplier))

        self.marionette.set_script_timeout(6000)
        result = self.marionette.execute_async_script("""
setTimeout(function() { marionetteScriptFinished(10%s); }, 5000);
""" % multiplier)
        self.assertEqual(result, 30 if self.content else 10)

class TestExecuteIsolationChrome(TestExecuteIsolationContent):
    def setUp(self):
        super(TestExecuteIsolationChrome, self).setUp()
        self.marionette.set_context("chrome")
        self.content = False


