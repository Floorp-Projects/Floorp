# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver.by import By
from marionette_driver.errors import (
    MarionetteException,
    NoSuchElementException,
    ScriptTimeoutException,
)
from marionette_driver.marionette import HTMLElement

from marionette_harness import MarionetteTestCase, run_if_manage_instance, skip_if_mobile


class TestTimeouts(MarionetteTestCase):
    def tearDown(self):
        self.marionette.timeout.reset()
        MarionetteTestCase.tearDown(self)

    def test_page_timeout_notdefinetimeout_pass(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)

    def test_page_timeout_fail(self):
        self.marionette.timeout.page_load = 0
        test_html = self.marionette.absolute_url("slow")
        with self.assertRaises(MarionetteException):
            self.marionette.navigate(test_html)

    def test_page_timeout_pass(self):
        self.marionette.timeout.page_load = 60
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)

    def test_search_timeout_notfound_settimeout(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        self.marionette.timeout.implicit = 1
        with self.assertRaises(NoSuchElementException):
            self.marionette.find_element(By.ID, "I'm not on the page")
        self.marionette.timeout.implicit = 0
        with self.assertRaises(NoSuchElementException):
            self.marionette.find_element(By.ID, "I'm not on the page")

    @skip_if_mobile("Bug 1317121 - android emulator is too slow")
    def test_search_timeout_found_settimeout(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        button = self.marionette.find_element(By.ID, "createDivButton")
        button.click()
        self.marionette.timeout.implicit = 8
        self.assertEqual(HTMLElement, type(self.marionette.find_element(By.ID, "newDiv")))

    @skip_if_mobile("Bug 1306848 - android emulator is too slow")
    def test_search_timeout_found(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        button = self.marionette.find_element(By.ID, "createDivButton")
        button.click()
        self.assertRaises(NoSuchElementException, self.marionette.find_element, By.ID, "newDiv")

    @run_if_manage_instance("Only runnable if Marionette manages the instance")
    @skip_if_mobile("Bug 1322993 - Missing temporary folder")
    def test_reset_timeout(self):
        timeouts = [getattr(self.marionette.timeout, f) for f in (
            'implicit', 'page_load', 'script',)]

        def do_check(callback):
            for timeout in timeouts:
                timeout = 10000
                self.assertEqual(timeout, 10000)
            callback()
            for timeout in timeouts:
                self.assertNotEqual(timeout, 10000)

        def callback_quit():
            self.marionette.quit()
            self.marionette.start_session()

        do_check(self.marionette.restart)
        do_check(callback_quit)

    def test_execute_async_timeout_settimeout(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        self.marionette.timeout.script = 1
        with self.assertRaises(ScriptTimeoutException):
            self.marionette.execute_async_script("var x = 1;")

    def test_no_timeout_settimeout(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        self.marionette.timeout.script = 1
        self.assertTrue(self.marionette.execute_async_script("""
             var callback = arguments[arguments.length - 1];
             setTimeout(function() { callback(true); }, 500);
             """))

    def test_deprecated_set_search_timeout(self):
        self.marionette.set_search_timeout(1000)
        self.assertEqual(1, self.marionette.timeout.implicit)

    def test_deprecated_set_script_timeout(self):
        self.marionette.set_script_timeout(2000)
        self.assertEqual(2, self.marionette.timeout.script)

    def test_deprecated_set_page_load_timeout(self):
        self.marionette.set_page_load_timeout(3000)
        self.assertEqual(3, self.marionette.timeout.page_load)
