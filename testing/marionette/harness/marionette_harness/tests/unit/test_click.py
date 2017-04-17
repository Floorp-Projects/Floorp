# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import urllib

from marionette_driver.by import By
from marionette_driver import errors
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


obscured_overlay = inline("""
<style>
* { margin: 0; padding: 0; }
body { height: 100vh }
#overlay {
  background-color: pink;
  position: absolute;
  width: 100%;
  height: 100%;
}
</style>

<div id=overlay></div>
<a id=obscured href=#>link</a>

<script>
window.clicked = false;

let link = document.querySelector("#obscured");
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
        self.marionette.navigate(inline("""
            <button>click me</button>
            <script>
              window.clicks = 0;
              let button = document.querySelector("button");
              button.addEventListener("click", () => window.clicks++);
            </script>
        """))
        button = self.marionette.find_element(By.TAG_NAME, "button")
        button.click()
        self.assertEqual(1, self.marionette.execute_script("return window.clicks", sandbox=None))

    def test_click_number_link(self):
        test_html = self.marionette.absolute_url("clicks.html")
        self.marionette.navigate(test_html)
        self.marionette.find_element(By.LINK_TEXT, "333333").click()
        # Bug 1335778 - Missing implicit wait for page being loaded
        Wait(self.marionette, timeout=self.marionette.timeout.page_load,
             ignored_exceptions=errors.NoSuchElementException).until(
            lambda m: m.find_element(By.ID, "username"),
            message="Username field hasn't been found")
        self.assertEqual(self.marionette.title, "XHTML Test Page")

    def test_clicking_an_element_that_is_not_displayed_raises(self):
        test_html = self.marionette.absolute_url("hidden.html")
        self.marionette.navigate(test_html)

        with self.assertRaises(errors.ElementNotInteractableException):
            self.marionette.find_element(By.ID, "child").click()

    def test_clicking_on_a_multiline_link(self):
        test_html = self.marionette.absolute_url("clicks.html")
        self.marionette.navigate(test_html)
        self.marionette.find_element(By.ID, "overflowLink").click()
        # Bug 1335778 - Missing implicit wait for page being loaded
        Wait(self.marionette, timeout=self.marionette.timeout.page_load,
             ignored_exceptions=errors.NoSuchElementException).until(
            lambda m: m.find_element(By.ID, "username"),
            message="Username field hasn't been found")
        self.assertEqual(self.marionette.title, "XHTML Test Page")

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
        self.marionette.navigate(obscured_overlay)
        overlay = self.marionette.find_element(By.ID, "overlay")
        obscured = self.marionette.find_element(By.ID, "obscured")

        overlay.click()
        with self.assertRaises(errors.ElementClickInterceptedException):
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

    def test_input_file(self):
        self.marionette.navigate(inline("<input type=file>"))
        with self.assertRaises(errors.InvalidArgumentException):
            self.marionette.find_element(By.TAG_NAME, "input").click()

    def test_container_element(self):
        self.marionette.navigate(inline("""
            <select>
              <option>foo</option>
            </select>"""))
        option = self.marionette.find_element(By.TAG_NAME, "option")
        option.click()
        self.assertTrue(option.get_property("selected"))

    def test_container_element_outside_view(self):
        self.marionette.navigate(inline("""
            <select style="margin-top: 100vh">
              <option>foo</option>
            </select>"""))
        option = self.marionette.find_element(By.TAG_NAME, "option")
        option.click()
        self.assertTrue(option.get_property("selected"))

    def test_obscured_element(self):
        self.marionette.navigate(obscured_overlay)
        overlay = self.marionette.find_element(By.ID, "overlay")
        obscured = self.marionette.find_element(By.ID, "obscured")

        overlay.click()
        with self.assertRaises(errors.ElementClickInterceptedException):
            obscured.click()
        self.assertFalse(self.marionette.execute_script("return window.clicked", sandbox=None))

    def test_pointer_events_none(self):
        self.marionette.navigate(inline("""
            <button style="pointer-events: none">click me</button>
            <script>
              window.clicked = false;
              let button = document.querySelector("button");
              button.addEventListener("click", () => window.clicked = true);
            </script>
        """))
        button = self.marionette.find_element(By.TAG_NAME, "button")
        self.assertEqual("none", button.value_of_css_property("pointer-events"))

        with self.assertRaisesRegexp(errors.ElementClickInterceptedException,
                                     "does not have pointer events enabled"):
            button.click()
        self.assertFalse(self.marionette.execute_script("return window.clicked", sandbox=None))
