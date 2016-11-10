# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase
from marionette_driver.marionette import HTMLElement
from marionette_driver.errors import (NoSuchElementException,
                                      MarionetteException,
                                      InvalidArgumentException,
                                      ScriptTimeoutException)
from marionette_driver.by import By


class TestTimeouts(MarionetteTestCase):
    def tearDown(self):
        self.marionette.reset_timeouts()
        MarionetteTestCase.tearDown(self)

    def test_page_timeout_notdefinetimeout_pass(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)

    def test_page_timeout_fail(self):
        self.marionette.set_page_load_timeout(0)
        test_html = self.marionette.absolute_url("test.html")
        self.assertRaises(MarionetteException, self.marionette.navigate, test_html)

    def test_page_timeout_pass(self):
        self.marionette.set_page_load_timeout(60000)
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)

    def test_search_timeout_notfound_settimeout(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        self.marionette.set_search_timeout(1000)
        self.assertRaises(NoSuchElementException, self.marionette.find_element, By.ID, "I'm not on the page")
        self.marionette.set_search_timeout(0)
        self.assertRaises(NoSuchElementException, self.marionette.find_element, By.ID, "I'm not on the page")

    def test_search_timeout_found_settimeout(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        button = self.marionette.find_element(By.ID, "createDivButton")
        button.click()
        self.marionette.set_search_timeout(8000)
        self.assertEqual(HTMLElement, type(self.marionette.find_element(By.ID, "newDiv")))

    def test_search_timeout_found(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        button = self.marionette.find_element(By.ID, "createDivButton")
        button.click()
        self.assertRaises(NoSuchElementException, self.marionette.find_element, By.ID, "newDiv")

    def test_execute_async_timeout_settimeout(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        self.marionette.set_script_timeout(10000)
        self.assertRaises(ScriptTimeoutException, self.marionette.execute_async_script, "var x = 1;")

    def test_no_timeout_settimeout(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        self.marionette.set_script_timeout(10000)
        self.assertTrue(self.marionette.execute_async_script("""
             var callback = arguments[arguments.length - 1];
             setTimeout(function() { callback(true); }, 500);
             """))

    def test_invalid_timeout_types(self):
        for val in [3.14, True, [], {}, "foo"]:
            print "testing %s" % type(val)
            self.assertRaises(InvalidArgumentException, self.marionette.set_search_timeout, val)
            self.assertRaises(InvalidArgumentException, self.marionette.set_script_timeout, val)
            self.assertRaises(InvalidArgumentException, self.marionette.set_page_load_timeout, val)

    def test_compat_input_types(self):
        # When using the spec-incompatible input format which we have
        # for backwards compatibility, it should be possible to send ms
        # as a string type and have the server parseInt it to an integer.
        body = {"type": "script", "ms": "30000"}
        self.marionette._send_message("setTimeouts", body)

    def test_deprecated_set_timeouts_command(self):
        body = {"implicit": 3000}
        self.marionette._send_message("timeouts", body)
