# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import urllib

from marionette_driver.by import By
from marionette_driver.errors import NoSuchElementException, ElementNotVisibleException
from marionette_driver.wait import Wait

from marionette_harness import MarionetteTestCase


def inline(doc):
    return "data:text/html;charset=utf-8,{}".format(urllib.quote(doc))


# The <a> element in the following HTML is not interactable because it
# is hidden by an overlay when scrolled into the top of the viewport.
# It should be interactable when scrolled in at the bottom of the
# viewport.
fixed_overlay = inline("""
<style>
* { margin: 0; padding: 0; }
body { height: 300vh }
div, a { display: block }
div {
  background-color: pink;
  position: fixed;
  width: 100%;
  height: 40px;
  top: 0;
}
a {
  margin-top: 1000px;
}
</style>

<div>overlay</div>
<a href=#>link</a>

<script>
window.clicked = false;

let link = document.querySelector("a");
link.addEventListener("click", () => window.clicked = true);
</script>
""")


class TestLegacyClick(MarionetteTestCase):
    """Uses legacy Selenium element displayedness checks."""

    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.delete_session()
        self.marionette.start_session()

    def test_click(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        link = self.marionette.find_element(By.ID, "mozLink")
        link.click()
        self.assertEqual("Clicked", self.marionette.execute_script("return document.getElementById('mozLink').innerHTML;"))

    def test_clicking_a_link_made_up_of_numbers_is_handled_correctly(self):
        test_html = self.marionette.absolute_url("clicks.html")
        self.marionette.navigate(test_html)
        self.marionette.find_element(By.LINK_TEXT, "333333").click()
        Wait(self.marionette, timeout=30, ignored_exceptions=NoSuchElementException).until(
            lambda m: m.find_element(By.ID, 'username'))
        self.assertEqual(self.marionette.title, "XHTML Test Page")

    def test_clicking_an_element_that_is_not_displayed_raises(self):
        test_html = self.marionette.absolute_url('hidden.html')
        self.marionette.navigate(test_html)

        with self.assertRaises(ElementNotVisibleException):
            self.marionette.find_element(By.ID, 'child').click()

    def test_clicking_on_a_multiline_link(self):
        test_html = self.marionette.absolute_url("clicks.html")
        self.marionette.navigate(test_html)
        self.marionette.find_element(By.ID, "overflowLink").click()
        self.wait_for_condition(lambda mn: self.marionette.title == "XHTML Test Page")

    def test_scroll_into_view_near_end(self):
        self.marionette.navigate(fixed_overlay)
        link = self.marionette.find_element(By.TAG_NAME, "a")
        link.click()
        self.assertTrue(self.marionette.execute_script("return window.clicked", sandbox=None))


class TestClick(TestLegacyClick):
    """Uses WebDriver specification compatible element interactability
    checks.
    """

    def setUp(self):
        TestLegacyClick.setUp(self)
        self.marionette.delete_session()
        self.marionette.start_session(
            {"requiredCapabilities": {"specificationLevel": 1}})

    def test_click_element_obscured_by_absolute_positioned_element(self):
        self.marionette.navigate(inline("""
            <style>
            * { margin: 0; padding: 0; }
            div { display: block; width: 100%; height: 100%; }
            #obscured { background-color: blue }
            #overlay { background-color: red; position: absolute; top: 0; }
            </style>

            <div id=obscured></div>
            <div id=overlay></div>"""))

        overlay = self.marionette.find_element(By.ID, "overlay")
        obscured = self.marionette.find_element(By.ID, "obscured")

        overlay.click()
        with self.assertRaises(ElementNotVisibleException):
            obscured.click()

    def test_centre_outside_viewport_vertically(self):
        self.marionette.navigate(inline("""
            <style>
            * { margin: 0; padding: 0; }
            div {
             display: block;
             position: absolute;
             background-color: blue;
             width: 200px;
             height: 200px;

             /* move centre point off viewport vertically */
             top: -105px;
            }
            </style>

            <div></div>"""))

        self.marionette.find_element(By.TAG_NAME, "div").click()

    def test_centre_outside_viewport_horizontally(self):
        self.marionette.navigate(inline("""
            <style>
            * { margin: 0; padding: 0; }
            div {
             display: block;
             position: absolute;
             background-color: blue;
             width: 200px;
             height: 200px;

             /* move centre point off viewport horizontally */
             left: -105px;
            }
            </style>

            <div></div>"""))

        self.marionette.find_element(By.TAG_NAME, "div").click()

    def test_centre_outside_viewport(self):
        self.marionette.navigate(inline("""
            <style>
            * { margin: 0; padding: 0; }
            div {
             display: block;
             position: absolute;
             background-color: blue;
             width: 200px;
             height: 200px;

             /* move centre point off viewport */
             left: -105px;
             top: -105px;
            }
            </style>

            <div></div>"""))

        self.marionette.find_element(By.TAG_NAME, "div").click()

    def test_css_transforms(self):
        self.marionette.navigate(inline("""
            <style>
            * { margin: 0; padding: 0; }
            div {
             display: block;
             background-color: blue;
             width: 200px;
             height: 200px;

             transform: translateX(-105px);
            }
            </style>

            <div></div>"""))

        self.marionette.find_element(By.TAG_NAME, "div").click()
