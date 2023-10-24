# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver.by import By
from marionette_driver.errors import NoSuchElementException
from marionette_driver.marionette import WebElement

from marionette_harness import MarionetteTestCase, parameterized, WindowManagerMixin


PAGE_XHTML = "chrome://remote/content/marionette/test_no_xul.xhtml"
PAGE_XUL = "chrome://remote/content/marionette/test.xhtml"


class TestElementIDChrome(WindowManagerMixin, MarionetteTestCase):
    def setUp(self):
        super(TestElementIDChrome, self).setUp()

        self.marionette.set_context("chrome")

    def tearDown(self):
        self.close_all_windows()

        super(TestElementIDChrome, self).tearDown()

    @parameterized("XUL", PAGE_XUL)
    @parameterized("XHTML", PAGE_XHTML)
    def test_id_identical_for_the_same_element(self, chrome_url):
        win = self.open_chrome_window(chrome_url)
        self.marionette.switch_to_window(win)

        found_el = self.marionette.find_element(By.ID, "textInput")
        self.assertEqual(WebElement, type(found_el))

        found_el_new = self.marionette.find_element(By.ID, "textInput")
        self.assertEqual(found_el_new.id, found_el.id)

    @parameterized("XUL", PAGE_XUL)
    @parameterized("XHTML", PAGE_XHTML)
    def test_id_unique_per_session(self, chrome_url):
        win = self.open_chrome_window(chrome_url)
        self.marionette.switch_to_window(win)

        found_el = self.marionette.find_element(By.ID, "textInput")
        self.assertEqual(WebElement, type(found_el))

        self.marionette.delete_session()
        self.marionette.start_session()

        self.marionette.set_context("chrome")
        self.marionette.switch_to_window(win)

        found_el_new = self.marionette.find_element(By.ID, "textInput")
        self.assertNotEqual(found_el_new.id, found_el.id)

    @parameterized("XUL", PAGE_XUL)
    @parameterized("XHTML", PAGE_XHTML)
    def test_id_no_such_element_in_another_chrome_window(self, chrome_url):
        original_handle = self.marionette.current_window_handle

        win = self.open_chrome_window(chrome_url)
        self.marionette.switch_to_window(win)

        found_el = self.marionette.find_element(By.ID, "textInput")
        self.assertEqual(WebElement, type(found_el))

        self.marionette.switch_to_window(original_handle)

        with self.assertRaises(NoSuchElementException):
            found_el.get_property("localName")

    @parameterized("XUL", PAGE_XUL)
    @parameterized("XHTML", PAGE_XHTML)
    def test_id_removed_when_chrome_window_is_closed(self, chrome_url):
        original_handle = self.marionette.current_window_handle

        win = self.open_chrome_window(chrome_url)
        self.marionette.switch_to_window(win)

        found_el = self.marionette.find_element(By.ID, "textInput")
        self.assertEqual(WebElement, type(found_el))

        self.marionette.close_chrome_window()
        self.marionette.switch_to_window(original_handle)

        with self.assertRaises(NoSuchElementException):
            found_el.get_property("localName")
