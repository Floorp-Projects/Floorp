# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver.by import By
from marionette_driver.errors import MoveTargetOutOfBoundsException

from marionette_harness import MarionetteTestCase, skip, skip_if_mobile


class TestClickScrolling(MarionetteTestCase):


    def test_clicking_on_anchor_scrolls_page(self):
        scrollScript = """
            var pageY;
            if (typeof(window.pageYOffset) == 'number') {
                pageY = window.pageYOffset;
            } else {
                pageY = document.documentElement.scrollTop;
            }
            return pageY;"""

        test_html = self.marionette.absolute_url("macbeth.html")
        self.marionette.navigate(test_html)

        self.marionette.find_element(By.PARTIAL_LINK_TEXT, "last speech").click()
        y_offset = self.marionette.execute_script(scrollScript)

        # Focusing on to click, but not actually following,
        # the link will scroll it in to view, which is a few
        # pixels further than 0

        self.assertTrue(y_offset > 300)

    def test_should_scroll_to_click_on_an_element_hidden_by_overflow(self):
        test_html = self.marionette.absolute_url("click_out_of_bounds_overflow.html")
        self.marionette.navigate(test_html)

        link = self.marionette.find_element(By.ID, "link")
        try:
            link.click()
        except MoveTargetOutOfBoundsException:
            self.fail("Should not be out of bounds")

    @skip("Bug 1200197 - Cannot interact with elements hidden inside overflow:scroll")
    def test_should_be_able_to_click_on_an_element_hidden_by_overflow(self):
        test_html = self.marionette.absolute_url("scroll.html")
        self.marionette.navigate(test_html)

        link = self.marionette.find_element(By.ID, "line8")
        link.click()
        self.assertEqual("line8", self.marionette.find_element(By.ID, "clicked").text)

    def test_should_not_scroll_overflow_elements_which_are_visible(self):
        test_html = self.marionette.absolute_url("scroll2.html")
        self.marionette.navigate(test_html)

        list_el = self.marionette.find_element(By.TAG_NAME, "ul")
        item = list_el.find_element(By.ID, "desired")
        item.click()
        y_offset = self.marionette.execute_script("return arguments[0].scrollTop;", script_args=[list_el])
        self.assertEqual(0, y_offset)

    def test_should_not_scroll_if_already_scrolled_and_element_is_in_view(self):
        test_html = self.marionette.absolute_url("scroll3.html")
        self.marionette.navigate(test_html)

        button1 = self.marionette.find_element(By.ID, "button1")
        button2 = self.marionette.find_element(By.ID, "button2")

        button2.click()
        scroll_top = self.marionette.execute_script("return document.body.scrollTop;")
        button1.click()

        self.assertEqual(scroll_top, self.marionette.execute_script("return document.body.scrollTop;"))

    def test_should_be_able_to_click_radio_button_scrolled_into_view(self):
        test_html = self.marionette.absolute_url("scroll4.html")
        self.marionette.navigate(test_html)

        # If we dont throw we are good
        self.marionette.find_element(By.ID, "radio").click()

    @skip_if_mobile("Bug 1293855 - Lists differ: [70, 70] != [70, 120]")
    def test_should_not_scroll_elements_if_click_point_is_in_view(self):
        test_html = self.marionette.absolute_url("element_outside_viewport.html")

        for s in ["top", "right", "bottom", "left"]:
            for p in ["50", "30"]:
                self.marionette.navigate(test_html)
                scroll = self.marionette.execute_script("return [window.scrollX, window.scrollY];")
                self.marionette.find_element(By.ID, "{0}-{1}".format(s, p)).click()
                self.assertEqual(scroll, self.marionette.execute_script("return [window.scrollX, window.scrollY];"))

    @skip("Bug 1003687")
    def test_should_scroll_overflow_elements_if_click_point_is_out_of_view_but_element_is_in_view(self):
        test_html = self.marionette.absolute_url("scroll5.html")
        self.marionette.navigate(test_html)

        self.marionette.find_element(By.ID, "inner").click()
        self.assertEqual("clicked", self.marionette.find_element(By.ID, "clicked").text)
