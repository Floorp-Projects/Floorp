# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from marionette_test import MarionetteTestCase
from marionette import HTMLElement
from marionette import NoSuchElementException, JavascriptException, MarionetteException, ScriptTimeoutException

class TestTimeouts(MarionetteTestCase):
    def test_pagetimeout_notdefinetimeout_pass(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)

    def test_pagetimeout_fail(self):
        self.marionette.timeouts("page load", 0)
        test_html = self.marionette.absolute_url("test.html")
        self.assertRaises(MarionetteException, self.marionette.navigate, test_html)

    def test_pagetimeout_pass(self):
        self.marionette.timeouts("page load", 60000)
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)

    def test_searchtimeout_notfound_settimeout(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        self.marionette.timeouts("implicit", 1000)
        self.assertRaises(NoSuchElementException, self.marionette.find_element, "id", "I'm not on the page")
        self.marionette.timeouts("implicit", 0)
        self.assertRaises(NoSuchElementException, self.marionette.find_element, "id", "I'm not on the page")

    def test_searchtimeout_found_settimeout(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        self.marionette.timeouts("implicit", 4000)
        self.assertEqual(HTMLElement, type(self.marionette.find_element("id", "newDiv")))

    def test_searchtimeout_found(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        self.assertRaises(NoSuchElementException, self.marionette.find_element, "id", "newDiv")

    def test_execute_async_timeout_settimeout(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        self.marionette.timeouts("script", 10000)
        self.assertRaises(ScriptTimeoutException, self.marionette.execute_async_script, "var x = 1;")

    def test_no_timeout_settimeout(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        self.marionette.timeouts("script", 10000)
        self.assertTrue(self.marionette.execute_async_script("""
             var callback = arguments[arguments.length - 1];
             setTimeout(function() { callback(true); }, 500);
             """))
