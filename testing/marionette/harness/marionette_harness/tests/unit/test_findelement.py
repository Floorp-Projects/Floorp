# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re

from six.moves.urllib.parse import quote

from marionette_driver.by import By
from marionette_driver.errors import NoSuchElementException, InvalidSelectorException
from marionette_driver.marionette import WebElement

from marionette_harness import MarionetteTestCase, skip


def inline(doc, doctype="html"):
    if doctype == "html":
        return "data:text/html;charset=utf-8,{}".format(quote(doc))
    elif doctype == "xhtml":
        return "data:application/xhtml+xml,{}".format(
            quote(
                r"""<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
    "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
  <head>
    <title>XHTML might be the future</title>
  </head>

  <body>
    {}
  </body>
</html>""".format(
                    doc
                )
            )
        )


id_html = inline("<p id=foo></p>", doctype="html")
id_xhtml = inline('<p id="foo"></p>', doctype="xhtml")
parent_child_html = inline("<div id=parent><p id=child></p></div>", doctype="html")
parent_child_xhtml = inline(
    '<div id="parent"><p id="child"></p></div>', doctype="xhtml"
)
children_html = inline("<div><p>foo <p>bar</div>", doctype="html")
children_xhtml = inline("<div><p>foo</p> <p>bar</p></div>", doctype="xhtml")
class_html = inline("<p class='foo bar'>", doctype="html")
class_xhtml = inline('<p class="foo bar"></p>', doctype="xhtml")
name_html = inline("<p name=foo>", doctype="html")
name_xhtml = inline('<p name="foo"></p>', doctype="xhtml")


class TestFindElementHTML(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.timeout.implicit = 0

    def test_id(self):
        self.marionette.navigate(id_html)
        expected = self.marionette.execute_script("return document.querySelector('p')")
        found = self.marionette.find_element(By.ID, "foo")
        self.assertIsInstance(found, WebElement)
        self.assertEqual(found, expected)

    def test_child_element(self):
        self.marionette.navigate(parent_child_html)
        parent = self.marionette.find_element(By.ID, "parent")
        child = self.marionette.find_element(By.ID, "child")
        found = parent.find_element(By.TAG_NAME, "p")
        self.assertEqual(found.tag_name, "p")
        self.assertIsInstance(found, WebElement)
        self.assertEqual(child, found)

    def test_tag_name(self):
        self.marionette.navigate(children_html)
        el = self.marionette.execute_script("return document.querySelector('p')")
        found = self.marionette.find_element(By.TAG_NAME, "p")
        self.assertIsInstance(found, WebElement)
        self.assertEqual(el, found)

    def test_class_name(self):
        self.marionette.navigate(class_html)
        el = self.marionette.execute_script("return document.querySelector('.foo')")
        found = self.marionette.find_element(By.CLASS_NAME, "foo")
        self.assertIsInstance(found, WebElement)
        self.assertEqual(el, found)

    def test_by_name(self):
        self.marionette.navigate(name_html)
        el = self.marionette.execute_script(
            "return document.querySelector('[name=foo]')"
        )
        found = self.marionette.find_element(By.NAME, "foo")
        self.assertIsInstance(found, WebElement)
        self.assertEqual(el, found)

    def test_css_selector(self):
        self.marionette.navigate(children_html)
        el = self.marionette.execute_script("return document.querySelector('p')")
        found = self.marionette.find_element(By.CSS_SELECTOR, "p")
        self.assertIsInstance(found, WebElement)
        self.assertEqual(el, found)

    def test_invalid_css_selector_should_throw(self):
        with self.assertRaises(InvalidSelectorException):
            self.marionette.find_element(By.CSS_SELECTOR, "#")

    def test_xpath(self):
        self.marionette.navigate(id_html)
        el = self.marionette.execute_script("return document.querySelector('#foo')")
        found = self.marionette.find_element(By.XPATH, "id('foo')")
        self.assertIsInstance(found, WebElement)
        self.assertEqual(el, found)

    def test_not_found(self):
        self.marionette.timeout.implicit = 0
        self.assertRaises(
            NoSuchElementException,
            self.marionette.find_element,
            By.CLASS_NAME,
            "cheese",
        )
        self.assertRaises(
            NoSuchElementException,
            self.marionette.find_element,
            By.CSS_SELECTOR,
            "cheese",
        )
        self.assertRaises(
            NoSuchElementException, self.marionette.find_element, By.ID, "cheese"
        )
        self.assertRaises(
            NoSuchElementException, self.marionette.find_element, By.LINK_TEXT, "cheese"
        )
        self.assertRaises(
            NoSuchElementException, self.marionette.find_element, By.NAME, "cheese"
        )
        self.assertRaises(
            NoSuchElementException,
            self.marionette.find_element,
            By.PARTIAL_LINK_TEXT,
            "cheese",
        )
        self.assertRaises(
            NoSuchElementException, self.marionette.find_element, By.TAG_NAME, "cheese"
        )
        self.assertRaises(
            NoSuchElementException, self.marionette.find_element, By.XPATH, "cheese"
        )

    def test_not_found_implicit_wait(self):
        self.marionette.timeout.implicit = 0.5
        self.assertRaises(
            NoSuchElementException,
            self.marionette.find_element,
            By.CLASS_NAME,
            "cheese",
        )
        self.assertRaises(
            NoSuchElementException,
            self.marionette.find_element,
            By.CSS_SELECTOR,
            "cheese",
        )
        self.assertRaises(
            NoSuchElementException, self.marionette.find_element, By.ID, "cheese"
        )
        self.assertRaises(
            NoSuchElementException, self.marionette.find_element, By.LINK_TEXT, "cheese"
        )
        self.assertRaises(
            NoSuchElementException, self.marionette.find_element, By.NAME, "cheese"
        )
        self.assertRaises(
            NoSuchElementException,
            self.marionette.find_element,
            By.PARTIAL_LINK_TEXT,
            "cheese",
        )
        self.assertRaises(
            NoSuchElementException, self.marionette.find_element, By.TAG_NAME, "cheese"
        )
        self.assertRaises(
            NoSuchElementException, self.marionette.find_element, By.XPATH, "cheese"
        )

    def test_not_found_from_element(self):
        self.marionette.timeout.implicit = 0
        self.marionette.navigate(id_html)
        el = self.marionette.find_element(By.ID, "foo")
        self.assertRaises(
            NoSuchElementException, el.find_element, By.CLASS_NAME, "cheese"
        )
        self.assertRaises(
            NoSuchElementException, el.find_element, By.CSS_SELECTOR, "cheese"
        )
        self.assertRaises(NoSuchElementException, el.find_element, By.ID, "cheese")
        self.assertRaises(
            NoSuchElementException, el.find_element, By.LINK_TEXT, "cheese"
        )
        self.assertRaises(NoSuchElementException, el.find_element, By.NAME, "cheese")
        self.assertRaises(
            NoSuchElementException, el.find_element, By.PARTIAL_LINK_TEXT, "cheese"
        )
        self.assertRaises(
            NoSuchElementException, el.find_element, By.TAG_NAME, "cheese"
        )
        self.assertRaises(NoSuchElementException, el.find_element, By.XPATH, "cheese")

    def test_not_found_implicit_wait_from_element(self):
        self.marionette.timeout.implicit = 0.5
        self.marionette.navigate(id_html)
        el = self.marionette.find_element(By.ID, "foo")
        self.assertRaises(
            NoSuchElementException, el.find_element, By.CLASS_NAME, "cheese"
        )
        self.assertRaises(
            NoSuchElementException, el.find_element, By.CSS_SELECTOR, "cheese"
        )
        self.assertRaises(NoSuchElementException, el.find_element, By.ID, "cheese")
        self.assertRaises(
            NoSuchElementException, el.find_element, By.LINK_TEXT, "cheese"
        )
        self.assertRaises(NoSuchElementException, el.find_element, By.NAME, "cheese")
        self.assertRaises(
            NoSuchElementException, el.find_element, By.PARTIAL_LINK_TEXT, "cheese"
        )
        self.assertRaises(
            NoSuchElementException, el.find_element, By.TAG_NAME, "cheese"
        )
        self.assertRaises(NoSuchElementException, el.find_element, By.XPATH, "cheese")

    def test_css_selector_scope_doesnt_start_at_rootnode(self):
        self.marionette.navigate(parent_child_html)
        el = self.marionette.find_element(By.ID, "child")
        parent = self.marionette.find_element(By.ID, "parent")
        found = parent.find_element(By.CSS_SELECTOR, "p")
        self.assertEqual(el, found)

    def test_unknown_selector(self):
        with self.assertRaises(InvalidSelectorException):
            self.marionette.find_element("foo", "bar")

    def test_invalid_xpath_selector(self):
        with self.assertRaises(InvalidSelectorException):
            self.marionette.find_element(By.XPATH, "count(//input)")
        with self.assertRaises(InvalidSelectorException):
            parent = self.marionette.execute_script("return document.documentElement")
            parent.find_element(By.XPATH, "count(//input)")

    def test_invalid_css_selector(self):
        with self.assertRaises(InvalidSelectorException):
            self.marionette.find_element(By.CSS_SELECTOR, "")
        with self.assertRaises(InvalidSelectorException):
            parent = self.marionette.execute_script("return document.documentElement")
            parent.find_element(By.CSS_SELECTOR, "")

    def test_finding_active_element_returns_element(self):
        self.marionette.navigate(id_html)
        active = self.marionette.execute_script("return document.activeElement")
        self.assertEqual(active, self.marionette.get_active_element())


class TestFindElementXHTML(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.timeout.implicit = 0

    def test_id(self):
        self.marionette.navigate(id_xhtml)
        expected = self.marionette.execute_script("return document.querySelector('p')")
        found = self.marionette.find_element(By.ID, "foo")
        self.assertIsInstance(found, WebElement)
        self.assertEqual(expected, found)

    def test_child_element(self):
        self.marionette.navigate(parent_child_xhtml)
        parent = self.marionette.find_element(By.ID, "parent")
        child = self.marionette.find_element(By.ID, "child")
        found = parent.find_element(By.TAG_NAME, "p")
        self.assertEqual(found.tag_name, "p")
        self.assertIsInstance(found, WebElement)
        self.assertEqual(child, found)

    def test_tag_name(self):
        self.marionette.navigate(children_xhtml)
        el = self.marionette.execute_script("return document.querySelector('p')")
        found = self.marionette.find_element(By.TAG_NAME, "p")
        self.assertIsInstance(found, WebElement)
        self.assertEqual(el, found)

    def test_class_name(self):
        self.marionette.navigate(class_xhtml)
        el = self.marionette.execute_script("return document.querySelector('.foo')")
        found = self.marionette.find_element(By.CLASS_NAME, "foo")
        self.assertIsInstance(found, WebElement)
        self.assertEqual(el, found)

    def test_by_name(self):
        self.marionette.navigate(name_xhtml)
        el = self.marionette.execute_script(
            "return document.querySelector('[name=foo]')"
        )
        found = self.marionette.find_element(By.NAME, "foo")
        self.assertIsInstance(found, WebElement)
        self.assertEqual(el, found)

    def test_css_selector(self):
        self.marionette.navigate(children_xhtml)
        el = self.marionette.execute_script("return document.querySelector('p')")
        found = self.marionette.find_element(By.CSS_SELECTOR, "p")
        self.assertIsInstance(found, WebElement)
        self.assertEqual(el, found)

    def test_xpath(self):
        self.marionette.navigate(id_xhtml)
        el = self.marionette.execute_script("return document.querySelector('#foo')")
        found = self.marionette.find_element(By.XPATH, "id('foo')")
        self.assertIsInstance(found, WebElement)
        self.assertEqual(el, found)

    def test_css_selector_scope_does_not_start_at_rootnode(self):
        self.marionette.navigate(parent_child_xhtml)
        el = self.marionette.find_element(By.ID, "child")
        parent = self.marionette.find_element(By.ID, "parent")
        found = parent.find_element(By.CSS_SELECTOR, "p")
        self.assertEqual(el, found)

    def test_active_element(self):
        self.marionette.navigate(id_xhtml)
        active = self.marionette.execute_script("return document.activeElement")
        self.assertEqual(active, self.marionette.get_active_element())


class TestFindElementsHTML(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.timeout.implicit = 0

    def assertItemsIsInstance(self, items, typ):
        for item in items:
            self.assertIsInstance(item, typ)

    def test_child_elements(self):
        self.marionette.navigate(children_html)
        parent = self.marionette.find_element(By.TAG_NAME, "div")
        children = self.marionette.find_elements(By.TAG_NAME, "p")
        found = parent.find_elements(By.TAG_NAME, "p")
        self.assertItemsIsInstance(found, WebElement)
        self.assertSequenceEqual(found, children)

    def test_tag_name(self):
        self.marionette.navigate(children_html)
        els = self.marionette.execute_script("return document.querySelectorAll('p')")
        found = self.marionette.find_elements(By.TAG_NAME, "p")
        self.assertItemsIsInstance(found, WebElement)
        self.assertSequenceEqual(els, found)

    def test_class_name(self):
        self.marionette.navigate(class_html)
        els = self.marionette.execute_script("return document.querySelectorAll('.foo')")
        found = self.marionette.find_elements(By.CLASS_NAME, "foo")
        self.assertItemsIsInstance(found, WebElement)
        self.assertSequenceEqual(els, found)

    def test_by_name(self):
        self.marionette.navigate(name_html)
        els = self.marionette.execute_script(
            "return document.querySelectorAll('[name=foo]')"
        )
        found = self.marionette.find_elements(By.NAME, "foo")
        self.assertItemsIsInstance(found, WebElement)
        self.assertSequenceEqual(els, found)

    def test_css_selector(self):
        self.marionette.navigate(children_html)
        els = self.marionette.execute_script("return document.querySelectorAll('p')")
        found = self.marionette.find_elements(By.CSS_SELECTOR, "p")
        self.assertItemsIsInstance(found, WebElement)
        self.assertSequenceEqual(els, found)

    def test_invalid_css_selector_should_throw(self):
        with self.assertRaises(InvalidSelectorException):
            self.marionette.find_elements(By.CSS_SELECTOR, "#")

    def test_xpath(self):
        self.marionette.navigate(children_html)
        els = self.marionette.execute_script("return document.querySelectorAll('p')")
        found = self.marionette.find_elements(By.XPATH, ".//p")
        self.assertItemsIsInstance(found, WebElement)
        self.assertSequenceEqual(els, found)

    def test_css_selector_scope_doesnt_start_at_rootnode(self):
        self.marionette.navigate(parent_child_html)
        els = self.marionette.find_elements(By.ID, "child")
        parent = self.marionette.find_element(By.ID, "parent")
        found = parent.find_elements(By.CSS_SELECTOR, "p")
        self.assertSequenceEqual(els, found)

    def test_unknown_selector(self):
        with self.assertRaises(InvalidSelectorException):
            self.marionette.find_elements("foo", "bar")

    def test_invalid_xpath_selector(self):
        with self.assertRaises(InvalidSelectorException):
            self.marionette.find_elements(By.XPATH, "count(//input)")
        with self.assertRaises(InvalidSelectorException):
            parent = self.marionette.execute_script("return document.documentElement")
            parent.find_elements(By.XPATH, "count(//input)")

    def test_invalid_css_selector(self):
        with self.assertRaises(InvalidSelectorException):
            self.marionette.find_elements(By.CSS_SELECTOR, "")
        with self.assertRaises(InvalidSelectorException):
            parent = self.marionette.execute_script("return document.documentElement")
            parent.find_elements(By.CSS_SELECTOR, "")


class TestFindElementsXHTML(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.timeout.implicit = 0

    def assertItemsIsInstance(self, items, typ):
        for item in items:
            self.assertIsInstance(item, typ)

    def test_child_elements(self):
        self.marionette.navigate(children_xhtml)
        parent = self.marionette.find_element(By.TAG_NAME, "div")
        children = self.marionette.find_elements(By.TAG_NAME, "p")
        found = parent.find_elements(By.TAG_NAME, "p")
        self.assertItemsIsInstance(found, WebElement)
        self.assertSequenceEqual(found, children)

    def test_tag_name(self):
        self.marionette.navigate(children_xhtml)
        els = self.marionette.execute_script("return document.querySelectorAll('p')")
        found = self.marionette.find_elements(By.TAG_NAME, "p")
        self.assertItemsIsInstance(found, WebElement)
        self.assertSequenceEqual(els, found)

    def test_class_name(self):
        self.marionette.navigate(class_xhtml)
        els = self.marionette.execute_script("return document.querySelectorAll('.foo')")
        found = self.marionette.find_elements(By.CLASS_NAME, "foo")
        self.assertItemsIsInstance(found, WebElement)
        self.assertSequenceEqual(els, found)

    def test_by_name(self):
        self.marionette.navigate(name_xhtml)
        els = self.marionette.execute_script(
            "return document.querySelectorAll('[name=foo]')"
        )
        found = self.marionette.find_elements(By.NAME, "foo")
        self.assertItemsIsInstance(found, WebElement)
        self.assertSequenceEqual(els, found)

    def test_css_selector(self):
        self.marionette.navigate(children_xhtml)
        els = self.marionette.execute_script("return document.querySelectorAll('p')")
        found = self.marionette.find_elements(By.CSS_SELECTOR, "p")
        self.assertItemsIsInstance(found, WebElement)
        self.assertSequenceEqual(els, found)

    @skip("XHTML namespace not yet supported")
    def test_xpath(self):
        self.marionette.navigate(children_xhtml)
        els = self.marionette.execute_script("return document.querySelectorAll('p')")
        found = self.marionette.find_elements(By.XPATH, "//xhtml:p")
        self.assertItemsIsInstance(found, WebElement)
        self.assertSequenceEqual(els, found)

    def test_css_selector_scope_doesnt_start_at_rootnode(self):
        self.marionette.navigate(parent_child_xhtml)
        els = self.marionette.find_elements(By.ID, "child")
        parent = self.marionette.find_element(By.ID, "parent")
        found = parent.find_elements(By.CSS_SELECTOR, "p")
        self.assertSequenceEqual(els, found)
