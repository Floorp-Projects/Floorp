# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_driver.errors import ScriptTimeoutException

from marionette_harness import MarionetteTestCase


class TestExecuteIsolationContent(MarionetteTestCase):
    def setUp(self):
        super(TestExecuteIsolationContent, self).setUp()
        self.content = True

    def test_execute_async_isolate(self):
        # Results from one execute call that has timed out should not
        # contaminate a future call.
        multiplier = "*3" if self.content else "*1"
        self.marionette.timeout.script = 0.5
        self.assertRaises(ScriptTimeoutException,
                          self.marionette.execute_async_script,
                          ("setTimeout(function() {{ arguments[0](5{}); }}, 3000);"
                              .format(multiplier)))

        self.marionette.timeout.script = 6
        result = self.marionette.execute_async_script("""
        let [resolve] = arguments;
        setTimeout(function() {{ resolve(10{}); }}, 5000);
        """.format(multiplier))
        self.assertEqual(result, 30 if self.content else 10)

class TestExecuteIsolationChrome(TestExecuteIsolationContent):
    def setUp(self):
        super(TestExecuteIsolationChrome, self).setUp()
        self.marionette.set_context("chrome")
        self.content = False


