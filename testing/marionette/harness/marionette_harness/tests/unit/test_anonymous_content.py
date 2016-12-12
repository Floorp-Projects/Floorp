# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from unittest import skip

from marionette_driver.by import By
from marionette_driver.errors import NoSuchElementException
from marionette_driver.expected import element_present
from marionette_driver.marionette import HTMLElement
from marionette_driver.wait import Wait

from marionette_harness import MarionetteTestCase, WindowManagerMixin


class TestAnonymousContent(WindowManagerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestAnonymousContent, self).setUp()
        self.marionette.set_context("chrome")

        def open_window_with_js():
            self.marionette.execute_script("""
              window.open('chrome://marionette/content/test_anonymous_content.xul',
                          'foo', 'chrome,centerscreen');
            """)

        new_window = self.open_window(trigger=open_window_with_js)
        self.marionette.switch_to_window(new_window)
        self.assertNotEqual(self.marionette.current_chrome_window_handle, self.start_window)

    def tearDown(self):
        self.close_all_windows()

    def test_switch_to_anonymous_frame(self):
        self.marionette.find_element(By.ID, "testAnonymousContentBox")
        anon_browser_el = self.marionette.find_element(By.ID, "browser")
        self.assertTrue("test_anonymous_content.xul" in self.marionette.get_url())
        self.marionette.switch_to_frame(anon_browser_el)
        self.assertTrue("test.xul" in self.marionette.get_url())
        self.marionette.find_element(By.ID, "testXulBox")
        self.assertRaises(NoSuchElementException, self.marionette.find_element, By.ID, "testAnonymousContentBox")

    @skip("Bug 1311657 - Opened chrome window cannot be closed after call to switch_to_frame")
    def test_switch_to_anonymous_iframe(self):
        self.marionette.find_element(By.ID, "testAnonymousContentBox")
        el = self.marionette.find_element(By.ID, "container2")
        anon_iframe_el = el.find_element(By.ANON_ATTRIBUTE, {"anonid": "iframe"})
        self.marionette.switch_to_frame(anon_iframe_el)
        self.assertTrue("test.xul" in self.marionette.get_url())
        self.marionette.find_element(By.ID, "testXulBox")
        self.assertRaises(NoSuchElementException, self.marionette.find_element, By.ID,
                          "testAnonymousContentBox")

    def test_find_anonymous_element_by_attribute(self):
        el = Wait(self.marionette).until(element_present(By.ID, "dia"))
        self.assertEquals(HTMLElement, type(el.find_element(By.ANON_ATTRIBUTE, {"anonid": "buttons"})))
        self.assertEquals(1, len(el.find_elements(By.ANON_ATTRIBUTE, {"anonid": "buttons"})))

        with self.assertRaises(NoSuchElementException):
            el.find_element(By.ANON_ATTRIBUTE, {"anonid": "nonexistent"})
        self.assertEquals([], el.find_elements(By.ANON_ATTRIBUTE, {"anonid": "nonexistent"}))

    def test_find_anonymous_children(self):
        el = Wait(self.marionette).until(element_present(By.ID, "dia"))
        self.assertEquals(HTMLElement, type(el.find_element(By.ANON, None)))
        self.assertEquals(2, len(el.find_elements(By.ANON, None)))

        el = self.marionette.find_element(By.ID, "framebox")
        with self.assertRaises(NoSuchElementException):
            el.find_element(By.ANON, None)
        self.assertEquals([], el.find_elements(By.ANON, None))
