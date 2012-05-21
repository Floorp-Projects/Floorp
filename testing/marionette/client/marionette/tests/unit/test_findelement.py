# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from marionette_test import MarionetteTestCase
from marionette import HTMLElement
from errors import NoSuchElementException

class TestElements(MarionetteTestCase):
    def test_id(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        el = self.marionette.execute_script("return window.document.getElementById('mozLink');")
        found_el = self.marionette.find_element("id", "mozLink")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el.id, found_el.id)

    def test_child_element(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        el = self.marionette.find_element("id", "divLink")
        div = self.marionette.find_element("id", "testDiv")
        found_el = div.find_element("tag name", "a")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el.id, found_el.id)

    def test_child_elements(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        el = self.marionette.find_element("id", "divLink2")
        div = self.marionette.find_element("id", "testDiv")
        found_els = div.find_elements("tag name", "a")
        self.assertTrue(el.id in [found_el.id for found_el in found_els])

    def test_tag_name(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        el = self.marionette.execute_script("return window.document.getElementsByTagName('body')[0];")
        found_el = self.marionette.find_element("tag name", "body")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el.id, found_el.id)
        found_el = self.marionette.find_elements("tag name", "body")[0]
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el.id, found_el.id)

    def test_class_name(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        el = self.marionette.execute_script("return window.document.getElementsByClassName('linkClass')[0];")
        found_el = self.marionette.find_element("class name", "linkClass")
        self.assertEqual(HTMLElement, type(found_el));
        self.assertEqual(el.id, found_el.id)
        found_el = self.marionette.find_elements("class name", "linkClass")[0]
        self.assertEqual(HTMLElement, type(found_el));
        self.assertEqual(el.id, found_el.id)

    def test_name(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        el = self.marionette.execute_script("return window.document.getElementsByName('myInput')[0];")
        found_el = self.marionette.find_element("name", "myInput")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el.id, found_el.id)
        found_el = self.marionette.find_elements("name", "myInput")[0]
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el.id, found_el.id)
    
    def test_selector(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        el = self.marionette.execute_script("return window.document.getElementById('testh1');")
        found_el = self.marionette.find_element("css selector", "h1")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el.id, found_el.id)
        found_el = self.marionette.find_elements("css selector", "h1")[0]
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el.id, found_el.id)

    def test_link_text(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        el = self.marionette.execute_script("return window.document.getElementById('mozLink');")
        found_el = self.marionette.find_element("link text", "Click me!")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el.id, found_el.id)
        found_el = self.marionette.find_elements("link text", "Click me!")[0]
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el.id, found_el.id)

    def test_partial_link_text(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        el = self.marionette.execute_script("return window.document.getElementById('mozLink');")
        found_el = self.marionette.find_element("partial link text", "Click m")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el.id, found_el.id)
        found_el = self.marionette.find_elements("partial link text", "Click m")[0]
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el.id, found_el.id)

    def test_xpath(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        el = self.marionette.execute_script("return window.document.getElementById('mozLink');")
        found_el = self.marionette.find_element("xpath", "id('mozLink')")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el.id, found_el.id)
        found_el = self.marionette.find_elements("xpath", "id('mozLink')")[0]
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el.id, found_el.id)

    def test_not_found(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        self.assertRaises(NoSuchElementException, self.marionette.find_element, "id", "I'm not on the page")

    def test_timeout(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        self.assertRaises(NoSuchElementException, self.marionette.find_element, "id", "newDiv")
        self.assertTrue(True, self.marionette.set_search_timeout(4000))
        self.marionette.navigate(test_html)
        self.assertEqual(HTMLElement, type(self.marionette.find_element("id", "newDiv")))

class TestElementsChrome(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.set_context("chrome")
        self.win = self.marionette.get_window()
        #need to get the file:// path for xul
        unit = os.path.abspath(os.path.join(os.path.realpath(__file__), os.path.pardir))
        tests = os.path.abspath(os.path.join(unit, os.path.pardir))
        mpath = os.path.abspath(os.path.join(tests, os.path.pardir))
        xul = "file://" + os.path.join(mpath, "www", "test.xul")
        self.marionette.execute_script("window.open('" + xul +"', '_blank', 'chrome,centerscreen');")

    def tearDown(self):
        self.marionette.execute_script("window.close();")
        self.marionette.switch_to_window(self.win)
        MarionetteTestCase.tearDown(self)

    def test_id(self):
        el = self.marionette.execute_script("return window.document.getElementById('textInput');")
        found_el = self.marionette.find_element("id", "textInput")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el.id, found_el.id)

    def test_child_element(self):
        el = self.marionette.find_element("id", "textInput")
        parent = self.marionette.find_element("id", "things")
        found_el = parent.find_element("tag name", "textbox")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el.id, found_el.id)

    def test_child_elements(self):
        el = self.marionette.find_element("id", "textInput3")
        parent = self.marionette.find_element("id", "things")
        found_els = parent.find_elements("tag name", "textbox")
        self.assertTrue(el.id in [found_el.id for found_el in found_els])

    def test_tag_name(self):
        el = self.marionette.execute_script("return window.document.getElementsByTagName('vbox')[0];")
        found_el = self.marionette.find_element("tag name", "vbox")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el.id, found_el.id)

    def test_class_name(self):
        el = self.marionette.execute_script("return window.document.getElementsByClassName('asdf')[0];")
        found_el = self.marionette.find_element("class name", "asdf")
        self.assertEqual(HTMLElement, type(found_el));
        self.assertEqual(el.id, found_el.id)

    def test_xpath(self):
        el = self.marionette.execute_script("return window.document.getElementById('testBox');")
        found_el = self.marionette.find_element("xpath", "id('testBox')")
        self.assertEqual(HTMLElement, type(found_el));
        self.assertEqual(el.id, found_el.id)

    def test_not_found(self):
        self.assertRaises(NoSuchElementException, self.marionette.find_element, "id", "I'm not on the page")


    def test_timeout(self):
        self.assertRaises(NoSuchElementException, self.marionette.find_element, "id", "myid")
        self.assertTrue(True, self.marionette.set_search_timeout(4000))
        self.marionette.execute_script("window.setTimeout(function() {var b = window.document.createElement('button'); b.id = 'myid'; document.getElementById('things').appendChild(b);}, 1000)")
        self.assertEqual(HTMLElement, type(self.marionette.find_element("id", "myid")))
        self.marionette.execute_script("window.document.getElementById('things').removeChild(window.document.getElementById('myid'));")
