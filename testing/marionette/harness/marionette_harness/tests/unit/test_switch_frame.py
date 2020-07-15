# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_driver.by import By
from marionette_driver.errors import InvalidArgumentException, NoSuchFrameException

from marionette_harness import MarionetteTestCase


class TestSwitchFrame(MarionetteTestCase):
    def setUp(self):
        super(TestSwitchFrame, self).setUp()

        test_html = self.marionette.absolute_url("frameset.html")
        self.marionette.navigate(test_html)

    def test_exceptions(self):
        frame = self.marionette.find_element(By.CSS_SELECTOR, ":root")
        with self.assertRaises(NoSuchFrameException):
            self.marionette.switch_to_frame(frame)

        with self.assertRaises(InvalidArgumentException):
            self.marionette.switch_to_frame(-1)

    def test_by_frame_element(self):
        frame = self.marionette.find_element(By.NAME, "third")
        self.marionette.switch_to_frame(frame)

        element = self.marionette.find_element(By.ID, "email")
        self.assertEquals(element.get_attribute("type"), "email")

    def test_by_index(self):
        self.marionette.switch_to_frame(2)

        element = self.marionette.find_element(By.ID, "email")
        self.assertEquals(element.get_attribute("type"), "email")

    def test_back_to_top_frame(self):
        frame1 = self.marionette.find_element(By.ID, "sixth")
        self.marionette.switch_to_frame(frame1)
        self.marionette.switch_to_frame(0)

        self.marionette.find_element(By.ID, "testDiv")

        self.marionette.switch_to_frame()
        frame = self.marionette.find_element(By.ID, "sixth")
        self.assertEquals(frame, frame1)


class TestSwitchParentFrame(MarionetteTestCase):
    def test_simple(self):
        frame_html = self.marionette.absolute_url("frameset.html")
        self.marionette.navigate(frame_html)
        frame = self.marionette.find_element(By.NAME, "third")
        self.marionette.switch_to_frame(frame)

        # If we don't find the following element we aren't on the right page
        self.marionette.find_element(By.ID, "checky")
        form_page_title = self.marionette.execute_script("return document.title")
        self.assertEqual("We Leave From Here", form_page_title)

        self.marionette.switch_to_parent_frame()

        current_page_title = self.marionette.execute_script("return document.title")
        self.assertEqual("Unique title", current_page_title)

    def test_from_default_context_is_a_noop(self):
        formpage = self.marionette.absolute_url("formPage.html")
        self.marionette.navigate(formpage)

        self.marionette.switch_to_parent_frame()

        form_page_title = self.marionette.execute_script("return document.title")
        self.assertEqual("We Leave From Here", form_page_title)

    def test_from_second_level(self):
        frame_html = self.marionette.absolute_url("frameset.html")
        self.marionette.navigate(frame_html)
        frame = self.marionette.find_element(By.NAME, "fourth")
        self.marionette.switch_to_frame(frame)

        second_level = self.marionette.find_element(By.NAME, "child1")
        self.marionette.switch_to_frame(second_level)
        self.marionette.find_element(By.NAME, "myCheckBox")

        self.marionette.switch_to_parent_frame()

        second_level = self.marionette.find_element(By.NAME, "child1")

    def test_from_iframe(self):
        frame_html = self.marionette.absolute_url("test_iframe.html")
        self.marionette.navigate(frame_html)
        frame = self.marionette.find_element(By.ID, "test_iframe")
        self.marionette.switch_to_frame(frame)

        current_page_title = self.marionette.execute_script("return document.title")
        self.assertEqual("Marionette Test", current_page_title)

        self.marionette.switch_to_parent_frame()

        parent_page_title = self.marionette.execute_script("return document.title")
        self.assertEqual("Marionette IFrame Test", parent_page_title)
