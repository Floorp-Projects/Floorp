# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import HTMLElement
from marionette_test import MarionetteTestCase
from errors import NoSuchElementException
from expected import element_present
from wait import Wait

class TestAnonymousContent(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.set_context("chrome")
        self.win = self.marionette.current_window_handle
        self.marionette.execute_script("window.open('chrome://marionette/content/test_anonymous_content.xul', 'foo', 'chrome,centerscreen');")
        self.marionette.switch_to_window('foo')
        self.assertNotEqual(self.win, self.marionette.current_window_handle)

    def tearDown(self):
        self.assertNotEqual(self.win, self.marionette.current_window_handle)
        self.marionette.execute_script("window.close();")
        self.marionette.switch_to_window(self.win)
        MarionetteTestCase.tearDown(self)

    def test_switch_to_anonymous_frame(self):
        self.marionette.find_element("id", "testAnonymousContentBox")
        anon_browser_el = self.marionette.find_element("id", "browser")
        self.assertTrue("test_anonymous_content.xul" in self.marionette.get_url())
        self.marionette.switch_to_frame(anon_browser_el)
        self.assertTrue("test.xul" in self.marionette.get_url())
        self.marionette.find_element("id", "testXulBox")
        self.assertRaises(NoSuchElementException, self.marionette.find_element, "id", "testAnonymousContentBox")

    def test_find_anonymous_element_by_attribute(self):
        el = Wait(self.marionette).until(element_present("id", "dia"))
        self.assertEquals(HTMLElement, type(el.find_element("anon attribute", {"anonid": "buttons"})))
        self.assertEquals(1, len(el.find_elements("anon attribute", {"anonid": "buttons"})))

        with self.assertRaises(NoSuchElementException):
            el.find_element("anon attribute", {"anonid": "nonexistent"})
        self.assertEquals([], el.find_elements("anon attribute", {"anonid": "nonexistent"}))

    def test_find_anonymous_children(self):
        el = Wait(self.marionette).until(element_present("id", "dia"))
        self.assertEquals(HTMLElement, type(el.find_element("anon", None)))
        self.assertEquals(2, len(el.find_elements("anon", None)))

        el = self.marionette.find_element("id", "framebox")
        with self.assertRaises(NoSuchElementException):
            el.find_element("anon", None)
        self.assertEquals([], el.find_elements("anon", None))
