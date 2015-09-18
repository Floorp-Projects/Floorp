# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase
from marionette_driver.marionette import HTMLElement
from marionette_driver.by import By
from marionette_driver.errors import NoSuchElementException, InvalidSelectorException


class TestElements(MarionetteTestCase):
    def test_id(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        el = self.marionette.execute_script("return window.document.getElementById('mozLink');")
        found_el = self.marionette.find_element(By.ID, "mozLink")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el, found_el)

    def test_child_element(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        el = self.marionette.find_element(By.ID, "divLink")
        div = self.marionette.find_element(By.ID, "testDiv")
        found_el = div.find_element(By.TAG_NAME, "a")
        self.assertEqual("a", found_el.tag_name)
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el, found_el)

    def test_child_elements(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        el = self.marionette.find_element(By.ID, "divLink2")
        div = self.marionette.find_element(By.ID, "testDiv")
        found_els = div.find_elements(By.TAG_NAME, "a")
        self.assertTrue(el.id in [found_el.id for found_el in found_els])

    def test_tag_name(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        el = self.marionette.execute_script("return window.document.getElementsByTagName('body')[0];")
        found_el = self.marionette.find_element(By.TAG_NAME, "body")
        self.assertEqual('body', found_el.tag_name)
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el, found_el)
        found_el = self.marionette.find_elements(By.TAG_NAME, "body")[0]
        self.assertEqual('body', found_el.tag_name)
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el, found_el)

    def test_class_name(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        el = self.marionette.execute_script("return window.document.getElementsByClassName('linkClass')[0];")
        found_el = self.marionette.find_element(By.CLASS_NAME, "linkClass")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el, found_el)
        found_el = self.marionette.find_elements(By.CLASS_NAME, "linkClass")[0]
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el, found_el)

    def test_by_name(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        el = self.marionette.execute_script("return window.document.getElementsByName('myInput')[0];")
        found_el = self.marionette.find_element(By.NAME, "myInput")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el, found_el)
        found_el = self.marionette.find_elements(By.NAME, "myInput")[0]
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el, found_el)

    def test_selector(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        el = self.marionette.execute_script("return window.document.getElementById('testh1');")
        found_el = self.marionette.find_element(By.CSS_SELECTOR, "h1")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el, found_el)
        found_el = self.marionette.find_elements(By.CSS_SELECTOR, "h1")[0]
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el, found_el)

    def test_link_text(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        el = self.marionette.execute_script("return window.document.getElementById('mozLink');")
        found_el = self.marionette.find_element(By.LINK_TEXT, "Click me!")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el, found_el)
        found_el = self.marionette.find_elements(By.LINK_TEXT, "Click me!")[0]
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el, found_el)

    def test_partial_link_text(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        el = self.marionette.execute_script("return window.document.getElementById('mozLink');")
        found_el = self.marionette.find_element(By.PARTIAL_LINK_TEXT, "Click m")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el, found_el)
        found_el = self.marionette.find_elements(By.PARTIAL_LINK_TEXT, "Click m")[0]
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el, found_el)

    def test_xpath(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        el = self.marionette.execute_script("return window.document.getElementById('mozLink');")
        found_el = self.marionette.find_element(By.XPATH, "id('mozLink')")
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el, found_el)
        found_el = self.marionette.find_elements(By.XPATH, "id('mozLink')")[0]
        self.assertEqual(HTMLElement, type(found_el))
        self.assertEqual(el, found_el)

    def test_not_found(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        self.marionette.set_search_timeout(1000)
        self.assertRaises(NoSuchElementException, self.marionette.find_element, By.ID, "I'm not on the page")
        self.marionette.set_search_timeout(0)
        self.assertRaises(NoSuchElementException, self.marionette.find_element, By.ID, "I'm not on the page")

    def test_timeout_element(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        button = self.marionette.find_element("id", "createDivButton")
        button.click()
        self.assertRaises(NoSuchElementException, self.marionette.find_element, By.ID, "newDiv")
        self.assertTrue(True, self.marionette.set_search_timeout(8000))
        self.assertEqual(HTMLElement, type(self.marionette.find_element(By.ID, "newDiv")))

    def test_timeout_elements(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        button = self.marionette.find_element("id", "createDivButton")
        button.click()
        self.assertEqual(len(self.marionette.find_elements(By.ID, "newDiv")), 0)
        self.assertTrue(True, self.marionette.set_search_timeout(8000))
        self.assertEqual(len(self.marionette.find_elements(By.ID, "newDiv")), 1)

    def test_css_selector_scope_doesnt_start_at_rootnode(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        el = self.marionette.find_element(By.ID, "mozLink")
        nav_el = self.marionette.find_element(By.ID, "testDiv")
        found_els = nav_el.find_elements(By.CSS_SELECTOR, "a")
        self.assertFalse(el.id in [found_el.id for found_el in found_els])

    def test_finding_active_element_returns_element(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        fbody = self.marionette.find_element(By.TAG_NAME, 'body')
        abody = self.marionette.get_active_element()
        self.assertEqual(fbody, abody)

    def test_throws_error_when_trying_to_use_invalid_selector_type(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        self.assertRaises(InvalidSelectorException, self.marionette.find_element, "Brie Search Type", "doesn't matter")

    def test_element_id_is_valid_uuid(self):
        import re
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        el = self.marionette.find_element(By.TAG_NAME, "body")
        uuid_regex = re.compile('^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$')
        self.assertIsNotNone(re.search(uuid_regex, el.id),
                             'UUID for the WebElement is not valid. ID is {}'\
                             .format(el.id))
    def test_should_find_elements_by_link_text(self):
        test_html = self.marionette.absolute_url("nestedElements.html")
        self.marionette.navigate(test_html)
        element = self.marionette.find_element(By.NAME, "div1")
        children = element.find_elements(By.LINK_TEXT, "hello world")
        self.assertEqual(len(children), 2)
        self.assertEqual("link1", children[0].get_attribute("name"))
        self.assertEqual("link2", children[1].get_attribute("name"))
