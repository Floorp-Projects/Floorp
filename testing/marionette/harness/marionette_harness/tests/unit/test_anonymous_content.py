# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_driver.by import By
from marionette_driver.errors import NoSuchElementException
from marionette_driver.marionette import HTMLElement

from marionette_harness import MarionetteTestCase, WindowManagerMixin


class TestAnonymousNodes(WindowManagerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestAnonymousNodes, self).setUp()
        self.marionette.set_context("chrome")

        def open_window_with_js():
            self.marionette.execute_script("""
              window.open('chrome://marionette/content/test_anonymous_content.xul',
                          'foo', 'chrome,centerscreen');
            """)

        new_window = self.open_window(trigger=open_window_with_js)
        self.marionette.switch_to_window(new_window)

    def tearDown(self):
        self.close_all_windows()

        super(TestAnonymousNodes, self).tearDown()

    def test_switch_to_anonymous_frame(self):
        self.marionette.find_element(By.ID, "testAnonymousContentBox")
        anon_browser_el = self.marionette.find_element(By.ID, "browser")
        self.assertTrue("test_anonymous_content.xul" in self.marionette.get_url())
        self.marionette.switch_to_frame(anon_browser_el)
        self.assertTrue("test.xul" in self.marionette.get_url())
        self.marionette.find_element(By.ID, "testXulBox")
        self.assertRaises(NoSuchElementException,
                          self.marionette.find_element, By.ID, "testAnonymousContentBox")

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
        accept_button = (By.ANON_ATTRIBUTE, {"dlgtype": "accept"},)
        not_existent = (By.ANON_ATTRIBUTE, {"anonid": "notexistent"},)

        # By using the window document element
        start_node = self.marionette.find_element(By.CSS_SELECTOR, ":root")
        button = start_node.find_element(*accept_button)
        self.assertEquals(HTMLElement, type(button))
        with self.assertRaises(NoSuchElementException):
            start_node.find_element(*not_existent)

        # By using the default start node
        self.assertEquals(button, self.marionette.find_element(*accept_button))
        with self.assertRaises(NoSuchElementException):
            self.marionette.find_element(*not_existent)

    def test_find_anonymous_elements_by_attribute(self):
        dialog_buttons = (By.ANON_ATTRIBUTE, {"anonid": "buttons"},)
        not_existent = (By.ANON_ATTRIBUTE, {"anonid": "notexistent"},)

        # By using the window document element
        start_node = self.marionette.find_element(By.CSS_SELECTOR, ":root")
        buttons = start_node.find_elements(*dialog_buttons)
        self.assertEquals(1, len(buttons))
        self.assertEquals(HTMLElement, type(buttons[0]))
        self.assertListEqual([], start_node.find_elements(*not_existent))

        # By using the default start node
        self.assertListEqual(buttons, self.marionette.find_elements(*dialog_buttons))
        self.assertListEqual([], self.marionette.find_elements(*not_existent))

    def test_find_anonymous_children(self):
        self.assertEquals(HTMLElement, type(self.marionette.find_element(By.ANON, None)))
        self.assertEquals(3, len(self.marionette.find_elements(By.ANON, None)))

        frame = self.marionette.find_element(By.ID, "framebox")
        with self.assertRaises(NoSuchElementException):
            frame.find_element(By.ANON, None)
        self.assertListEqual([], frame.find_elements(By.ANON, None))
