# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase
from marionette_driver.marionette import HTMLElement
from marionette_driver.errors import (NoSuchElementException,
                                      MarionetteException,
                                      ScriptTimeoutException)
from marionette_driver.by import By


class TestTimeouts(MarionetteTestCase):

    def tearDown(self):
        self.marionette.reset_timeouts()

        MarionetteTestCase.tearDown(self)

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
        self.assertRaises(NoSuchElementException, self.marionette.find_element, By.ID, "I'm not on the page")
        self.marionette.timeouts("implicit", 0)
        self.assertRaises(NoSuchElementException, self.marionette.find_element, By.ID, "I'm not on the page")

    def test_searchtimeout_found_settimeout(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        button = self.marionette.find_element(By.ID, "createDivButton")
        button.click()
        self.marionette.timeouts("implicit", 8000)
        self.assertEqual(HTMLElement, type(self.marionette.find_element(By.ID, "newDiv")))

    def test_searchtimeout_found(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        button = self.marionette.find_element(By.ID, "createDivButton")
        button.click()
        self.assertRaises(NoSuchElementException, self.marionette.find_element, By.ID, "newDiv")

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

    def test_invalid_timeout_type(self):
        self.assertRaises(ValueError, self.marionette.timeouts, "foobar", 1000)
        self.assertRaises(ValueError, self.marionette.timeouts, 42, 1000)
        self.assertRaises(MarionetteException, self.marionette.timeouts, "page load", "foobar")
