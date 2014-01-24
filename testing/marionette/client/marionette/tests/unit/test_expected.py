# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import urllib

import expected
import marionette_test

from by import By

def inline(doc):
    return "data:text/html;charset=utf-8,%s" % urllib.quote(doc)

static_element = inline("""<p>foo</p>""")
static_elements = static_element + static_element

remove_element_by_tag_name = \
    """var el = document.getElementsByTagName('%s')[0];
    document.getElementsByTagName("body")[0].remove(el);"""

hidden_element = inline("<p style='display: none'>hidden</p>")

selected_element = inline("<option selected>selected</option>")
unselected_element = inline("<option>unselected</option>")

enabled_element = inline("<input>")
disabled_element = inline("<input disabled>")

def no_such_element(marionette):
    return marionette.find_element(By.ID, "nosuchelement")

def no_such_elements(marionette):
    return marionette.find_elements(By.ID, "nosuchelement")

def p(marionette):
    return marionette.find_element(By.TAG_NAME, "p")

def ps(marionette):
    return marionette.find_elements(By.TAG_NAME, "p")

class TestExpected(marionette_test.MarionetteTestCase):
    def test_element_present_func(self):
        self.marionette.navigate(static_element)
        el = expected.element_present(p)(self.marionette)
        self.assertIsNotNone(el)

    def test_element_present_locator(self):
        self.marionette.navigate(static_element)
        el = expected.element_present(By.TAG_NAME, "p")(self.marionette)
        self.assertIsNotNone(el)

    def test_element_present_not_present(self):
        r = expected.element_present(no_such_element)(self.marionette)
        self.assertIsInstance(r, bool)
        self.assertFalse(r)

    def test_element_not_present_func(self):
        r = expected.element_not_present(no_such_element)(self.marionette)
        self.assertIsInstance(r, bool)
        self.assertTrue(r)

    def test_element_not_present_locator(self):
        r = expected.element_not_present(By.ID, "nosuchelement")(self.marionette)
        self.assertIsInstance(r, bool)
        self.assertTrue(r)

    def test_element_not_present_is_present(self):
        self.marionette.navigate(static_element)
        r = expected.element_not_present(p)(self.marionette)
        self.assertIsInstance(r, bool)
        self.assertFalse(r)

    def test_element_stale(self):
        self.marionette.navigate(static_element)
        el = self.marionette.find_element(By.TAG_NAME, "p")
        self.assertIsNotNone(el)
        self.marionette.execute_script(remove_element_by_tag_name % "p")
        r = expected.element_stale(el)(self.marionette)
        self.assertTrue(r)

    def test_element_stale_is_not_stale(self):
        self.marionette.navigate(static_element)
        el = self.marionette.find_element(By.TAG_NAME, "p")
        r = expected.element_stale(el)(self.marionette)
        self.assertFalse(r)

    def test_elements_present_func(self):
        self.marionette.navigate(static_elements)
        els = expected.elements_present(ps)(self.marionette)
        self.assertEqual(len(els), 2)

    def test_elements_present_locator(self):
        self.marionette.navigate(static_elements)
        els = expected.elements_present(By.TAG_NAME, "p")(self.marionette)
        self.assertEqual(len(els), 2)

    def test_elements_not_present_func(self):
        r = expected.element_not_present(no_such_elements)(self.marionette)
        self.assertIsInstance(r, bool)
        self.assertTrue(r)

    def test_elements_not_present_locator(self):
        r = expected.element_not_present(By.ID, "nosuchelement")(self.marionette)
        self.assertIsInstance(r, bool)
        self.assertTrue(r)

    def test_elements_not_present_is_present(self):
        self.marionette.navigate(static_elements)
        r = expected.elements_not_present(ps)(self.marionette)
        self.assertIsInstance(r, bool)
        self.assertFalse(r)

    def test_element_displayed(self):
        self.marionette.navigate(static_element)
        el = self.marionette.find_element(By.TAG_NAME, "p")
        visible = expected.element_displayed(el)(self.marionette)
        self.assertTrue(visible)

    def test_element_displayed_when_hidden(self):
        self.marionette.navigate(hidden_element)
        el = self.marionette.find_element(By.TAG_NAME, "p")
        hidden = expected.element_displayed(el)(self.marionette)
        self.assertFalse(hidden)

    def test_element_displayed_when_stale_element(self):
        self.marionette.navigate(static_element)
        el = self.marionette.find_element(By.TAG_NAME, "p")
        self.marionette.navigate("about:blank")
        missing = expected.element_displayed(el)(self.marionette)
        self.assertFalse(missing)

    def test_element_not_displayed(self):
        self.marionette.navigate(hidden_element)
        el = self.marionette.find_element(By.TAG_NAME, "p")
        visible = expected.element_not_displayed(el)(self.marionette)
        self.assertTrue(visible)

    def test_element_not_displayed_when_visible(self):
        self.marionette.navigate(static_element)
        el = self.marionette.find_element(By.TAG_NAME, "p")
        hidden = expected.element_not_displayed(el)(self.marionette)
        self.assertFalse(hidden)

    def test_element_not_displayed_when_stale_element(self):
        self.marionette.navigate(static_element)
        el = self.marionette.find_element(By.TAG_NAME, "p")
        self.marionette.navigate("about:blank")
        missing = expected.element_not_displayed(el)(self.marionette)
        self.assertTrue(missing)

    def test_element_selected(self):
        self.marionette.navigate(selected_element)
        el = self.marionette.find_element(By.TAG_NAME, "option")
        selected = expected.element_selected(el)(self.marionette)
        self.assertTrue(selected)

    def test_element_selected_when_not_selected(self):
        self.marionette.navigate(unselected_element)
        el = self.marionette.find_element(By.TAG_NAME, "option")
        unselected = expected.element_selected(el)(self.marionette)
        self.assertFalse(unselected)

    def test_element_not_selected(self):
        self.marionette.navigate(unselected_element)
        el = self.marionette.find_element(By.TAG_NAME, "option")
        unselected = expected.element_not_selected(el)(self.marionette)
        self.assertTrue(unselected)

    def test_element_not_selected_when_selected(self):
        self.marionette.navigate(selected_element)
        el = self.marionette.find_element(By.TAG_NAME, "option")
        selected = expected.element_not_selected(el)(self.marionette)
        self.assertFalse(selected)

    def test_element_enabled(self):
        self.marionette.navigate(enabled_element)
        el = self.marionette.find_element(By.TAG_NAME, "input")
        enabled = expected.element_enabled(el)(self.marionette)
        self.assertTrue(enabled)

    def test_element_enabled_when_disabled(self):
        self.marionette.navigate(disabled_element)
        el = self.marionette.find_element(By.TAG_NAME, "input")
        disabled = expected.element_enabled(el)(self.marionette)
        self.assertFalse(disabled)

    def test_element_not_enabled(self):
        self.marionette.navigate(disabled_element)
        el = self.marionette.find_element(By.TAG_NAME, "input")
        disabled = expected.element_not_enabled(el)(self.marionette)
        self.assertTrue(disabled)

    def test_element_not_enabled_when_enabled(self):
        self.marionette.navigate(enabled_element)
        el = self.marionette.find_element(By.TAG_NAME, "input")
        enabled = expected.element_not_enabled(el)(self.marionette)
        self.assertFalse(enabled)
