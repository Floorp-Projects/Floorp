# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_driver.by import By
from marionette_driver.errors import NoSuchElementException
from marionette_driver.marionette import HTMLElement

from marionette_harness import MarionetteTestCase, WindowManagerMixin


class TestElementsChrome(WindowManagerMixin, MarionetteTestCase):
    def setUp(self):
        super(TestElementsChrome, self).setUp()

        self.marionette.set_context("chrome")

        win = self.open_chrome_window("chrome://remote/content/marionette/test.xhtml")
        self.marionette.switch_to_window(win)

    def tearDown(self):
        self.close_all_windows()

        super(TestElementsChrome, self).tearDown()

    def test_id(self):
        el = self.marionette.execute_script(
            "return window.document.getElementById('textInput');"
        )
        found_el = self.marionette.find_element(By.ID, "textInput")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el, found_el)

    def test_that_we_can_find_elements_from_css_selectors(self):
        el = self.marionette.execute_script(
            "return window.document.getElementById('textInput');"
        )
        found_el = self.marionette.find_element(By.CSS_SELECTOR, "#textInput")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el, found_el)

    def test_child_element(self):
        el = self.marionette.find_element(By.ID, "textInput")
        parent = self.marionette.find_element(By.ID, "things")
        found_el = parent.find_element(By.TAG_NAME, "input")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el, found_el)

    def test_child_elements(self):
        el = self.marionette.find_element(By.ID, "textInput3")
        parent = self.marionette.find_element(By.ID, "things")
        found_els = parent.find_elements(By.TAG_NAME, "input")
        self.assertTrue(el.id in [found_el.id for found_el in found_els])

    def test_tag_name(self):
        el = self.marionette.execute_script(
            "return window.document.getElementsByTagName('vbox')[0];"
        )
        found_el = self.marionette.find_element(By.TAG_NAME, "vbox")
        self.assertEqual("vbox", found_el.tag_name)
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el, found_el)

    def test_class_name(self):
        el = self.marionette.execute_script(
            "return window.document.getElementsByClassName('asdf')[0];"
        )
        found_el = self.marionette.find_element(By.CLASS_NAME, "asdf")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el, found_el)

    def test_xpath(self):
        el = self.marionette.execute_script(
            "return window.document.getElementById('testBox');"
        )
        found_el = self.marionette.find_element(By.XPATH, "id('testBox')")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el, found_el)

    def test_not_found(self):
        self.marionette.timeout.implicit = 1
        self.assertRaises(
            NoSuchElementException,
            self.marionette.find_element,
            By.ID,
            "I'm not on the page",
        )
        self.marionette.timeout.implicit = 0
        self.assertRaises(
            NoSuchElementException,
            self.marionette.find_element,
            By.ID,
            "I'm not on the page",
        )

    def test_timeout(self):
        self.assertRaises(
            NoSuchElementException, self.marionette.find_element, By.ID, "myid"
        )
        self.marionette.timeout.implicit = 4
        self.marionette.execute_script(
            """
            window.setTimeout(function () {
              var b = window.document.createXULElement('button');
              b.id = 'myid';
              document.getElementById('things').appendChild(b);
            }, 1000); """
        )
        self.assertEqual(HTMLElement, type(self.marionette.find_element(By.ID, "myid")))
        self.marionette.execute_script(
            """
            var elem = window.document.getElementById('things');
            elem.removeChild(window.document.getElementById('myid')); """
        )
