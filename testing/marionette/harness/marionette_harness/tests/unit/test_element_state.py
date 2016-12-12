# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import types
import urllib

from marionette_driver.by import By

from marionette_harness import MarionetteTestCase


boolean_attributes = {
  "audio": ["autoplay", "controls", "loop", "muted"],
  "button": ["autofocus", "disabled", "formnovalidate"],
  "details": ["open"],
  "dialog": ["open"],
  "fieldset": ["disabled"],
  "form": ["novalidate"],
  "iframe": ["allowfullscreen"],
  "img": ["ismap"],
  "input": ["autofocus", "checked", "disabled", "formnovalidate", "multiple", "readonly", "required"],
  "menuitem": ["checked", "default", "disabled"],
  "object": ["typemustmatch"],
  "ol": ["reversed"],
  "optgroup": ["disabled"],
  "option": ["disabled", "selected"],
  "script": ["async", "defer"],
  "select": ["autofocus", "disabled", "multiple", "required"],
  "textarea": ["autofocus", "disabled", "readonly", "required"],
  "track": ["default"],
  "video": ["autoplay", "controls", "loop", "muted"],
}


def inline(doc, doctype="html"):
    if doctype == "html":
        return "data:text/html;charset=utf-8,{}".format(urllib.quote(doc))
    elif doctype == "xhtml":
        return "data:application/xhtml+xml,{}".format(urllib.quote(
r"""<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
    "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
  <head>
    <title>XHTML might be the future</title>
  </head>

  <body>
    {}
  </body>
</html>""".format(doc)))


attribute = inline("<input foo=bar>")
input = inline("<input>")
disabled = inline("<input disabled=baz>")
check = inline("<input type=checkbox>")


class TestIsElementEnabled(MarionetteTestCase):
    def test_is_enabled(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        l = self.marionette.find_element(By.NAME, "myCheckBox")
        self.assertTrue(l.is_enabled())
        self.marionette.execute_script("arguments[0].disabled = true;", [l])
        self.assertFalse(l.is_enabled())


class TestIsElementDisplayed(MarionetteTestCase):
    def test_is_displayed(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        l = self.marionette.find_element(By.NAME, "myCheckBox")
        self.assertTrue(l.is_displayed())
        self.marionette.execute_script("arguments[0].hidden = true;", [l])
        self.assertFalse(l.is_displayed())


class TestGetElementAttribute(MarionetteTestCase):
    def test_normal_attribute(self):
        self.marionette.navigate(inline("<p style=foo>"))
        el = self.marionette.find_element(By.TAG_NAME, "p")
        attr = el.get_attribute("style")
        self.assertIsInstance(attr, types.StringTypes)
        self.assertEqual("foo", attr)

    def test_boolean_attributes(self):
        for tag, attrs in boolean_attributes.iteritems():
            for attr in attrs:
                print("testing boolean attribute <{0} {1}>".format(tag, attr))
                doc = inline("<{0} {1}>".format(tag, attr))
                self.marionette.navigate(doc)
                el = self.marionette.find_element(By.TAG_NAME, tag)
                res = el.get_attribute(attr)
                self.assertIsInstance(res, types.StringTypes)
                self.assertEqual("true", res)

    def test_global_boolean_attributes(self):
        self.marionette.navigate(inline("<p hidden>foo"))
        el = self.marionette.find_element(By.TAG_NAME, "p")
        attr = el.get_attribute("hidden")
        self.assertIsInstance(attr, types.StringTypes)
        self.assertEqual("true", attr)

        self.marionette.navigate(inline("<p>foo"))
        el = self.marionette.find_element(By.TAG_NAME, "p")
        attr = el.get_attribute("hidden")
        self.assertIsNone(attr)

        self.marionette.navigate(inline("<p itemscope>foo"))
        el = self.marionette.find_element(By.TAG_NAME, "p")
        attr = el.get_attribute("itemscope")
        self.assertIsInstance(attr, types.StringTypes)
        self.assertEqual("true", attr)

        self.marionette.navigate(inline("<p>foo"))
        el = self.marionette.find_element(By.TAG_NAME, "p")
        attr = el.get_attribute("itemscope")
        self.assertIsNone(attr)

    # TODO(ato): Test for custom elements

    def test_xhtml(self):
        doc = inline("<p hidden=\"true\">foo</p>", doctype="xhtml")
        self.marionette.navigate(doc)
        el = self.marionette.find_element(By.TAG_NAME, "p")
        attr = el.get_attribute("hidden")
        self.assertIsInstance(attr, types.StringTypes)
        self.assertEqual("true", attr)


class TestGetElementProperty(MarionetteTestCase):
    def test_get(self):
        self.marionette.navigate(disabled)
        el = self.marionette.find_element(By.TAG_NAME, "input")
        prop = el.get_property("disabled")
        self.assertIsInstance(prop, bool)
        self.assertTrue(prop)

    def test_missing_property_returns_default(self):
        self.marionette.navigate(input)
        el = self.marionette.find_element(By.TAG_NAME, "input")
        prop = el.get_property("checked")
        self.assertIsInstance(prop, bool)
        self.assertFalse(prop)

    def test_attribute_not_returned(self):
        self.marionette.navigate(attribute)
        el = self.marionette.find_element(By.TAG_NAME, "input")
        self.assertEqual(el.get_property("foo"), None)

    def test_manipulated_element(self):
        self.marionette.navigate(check)
        el = self.marionette.find_element(By.TAG_NAME, "input")
        self.assertEqual(el.get_property("checked"), False)

        el.click()
        self.assertEqual(el.get_property("checked"), True)

        el.click()
        self.assertEqual(el.get_property("checked"), False)
