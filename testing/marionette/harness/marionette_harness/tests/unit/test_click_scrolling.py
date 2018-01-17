# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import urllib

from marionette_driver.by import By
from marionette_driver.errors import MoveTargetOutOfBoundsException

from marionette_harness import MarionetteTestCase, skip_if_mobile


def inline(doc):
    return "data:text/html;charset=utf-8,{}".format(urllib.quote(doc))


class TestClickScrolling(MarionetteTestCase):

    def test_clicking_on_anchor_scrolls_page(self):
        self.marionette.navigate(inline("""
          <a href="#content">Link to content</a>
          <div id="content" style="margin-top: 205vh;">Text</div>
        """))

        # Focusing on to click, but not actually following,
        # the link will scroll it in to view, which is a few
        # pixels further than 0
        self.marionette.find_element(By.CSS_SELECTOR, "a").click()

        y_offset = self.marionette.execute_script("""
          var pageY;
          if (typeof(window.pageYOffset) == 'number') {
              pageY = window.pageYOffset;
          } else {
            pageY = document.documentElement.scrollTop;
          }
          return pageY;
        """)

        self.assertGreater(y_offset, 300)

    def test_should_scroll_to_click_on_an_element_hidden_by_overflow(self):
        test_html = self.marionette.absolute_url("click_out_of_bounds_overflow.html")
        self.marionette.navigate(test_html)

        link = self.marionette.find_element(By.ID, "link")
        try:
            link.click()
        except MoveTargetOutOfBoundsException:
            self.fail("Should not be out of bounds")

    @skip_if_mobile("Bug 1293855 - Lists differ: [70, 70] != [70, 120]")
    def test_should_not_scroll_elements_if_click_point_is_in_view(self):
        test_html = self.marionette.absolute_url("element_outside_viewport.html")

        for s in ["top", "right", "bottom", "left"]:
            for p in ["50", "30"]:
                self.marionette.navigate(test_html)
                scroll = self.marionette.execute_script("return [window.scrollX, window.scrollY];")
                self.marionette.find_element(By.ID, "{0}-{1}".format(s, p)).click()
                self.assertEqual(scroll, self.marionette.execute_script(
                    "return [window.scrollX, window.scrollY];"))

    def test_do_not_scroll_again_if_element_is_already_in_view(self):
        self.marionette.navigate(inline("""
          <div style="height: 200vh;">
            <button id="button1" style="margin-top: 105vh">Button1</button>
            <button id="button2" style="position: relative; top: 5em">Button2</button>
          </div>
        """))
        button1 = self.marionette.find_element(By.ID, "button1")
        button2 = self.marionette.find_element(By.ID, "button2")

        button2.click()
        scroll_top = self.marionette.execute_script("return document.body.scrollTop;")
        button1.click()

        self.assertEqual(scroll_top, self.marionette.execute_script(
            "return document.body.scrollTop;"))

    def test_scroll_radio_button_into_view(self):
        self.marionette.navigate(inline("""
          <input type="radio" id="radio" style="margin-top: 105vh;">
        """))
        self.marionette.find_element(By.ID, "radio").click()

    def test_overflow_scroll_do_not_scroll_elements_which_are_visible(self):
        self.marionette.navigate(inline("""
          <ul style='overflow: scroll; height: 8em; line-height: 3em'>
            <li></li>
            <li id="desired">Text</li>
            <li></li>
            <li></li>
          </ul>
        """))

        list_el = self.marionette.find_element(By.TAG_NAME, "ul")
        expected_y_offset = self.marionette.execute_script(
            "return arguments[0].scrollTop;", script_args=(list_el,))

        item = list_el.find_element(By.ID, "desired")
        item.click()

        y_offset = self.marionette.execute_script("return arguments[0].scrollTop;",
                                                  script_args=(list_el,))
        self.assertEqual(expected_y_offset, y_offset)

    def test_overflow_scroll_click_on_hidden_element(self):
        self.marionette.navigate(inline("""
          Result: <span id="result"></span>
          <ul style='overflow: scroll; width: 150px; height: 8em; line-height: 4em'
              onclick="document.getElementById('result').innerText = event.target.id;">
            <li>line1</li>
            <li>line2</li>
            <li>line3</li>
            <li id="line4">line4</li>
          </ul>
        """))

        self.marionette.find_element(By.ID, "line4").click()
        self.assertEqual("line4", self.marionette.find_element(By.ID, "result").text)

    def test_overflow_scroll_vertically_for_click_point_outside_of_viewport(self):
        self.marionette.navigate(inline("""
          Result: <span id="result"></span>
          <div style='overflow: scroll; width: 100px; height: 100px; background-color: yellow;'>
            <div id="inner" style="width: 100px; height: 300px; background-color: green;"
                 onclick="document.getElementById('result').innerText = event.type" ></div>
          </div>
        """))

        self.marionette.find_element(By.ID, "inner").click()
        self.assertEqual("click", self.marionette.find_element(By.ID, "result").text)
