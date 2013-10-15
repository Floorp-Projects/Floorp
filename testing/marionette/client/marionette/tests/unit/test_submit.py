# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import time
from marionette_test import MarionetteTestCase
from errors import NoSuchElementException


class TestSubmit(MarionetteTestCase):

    def test_should_be_able_to_submit_forms(self):
        test_html = self.marionette.absolute_url("formPage.html")
        self.marionette.navigate(test_html)
        self.marionette.find_element("name", "login").submit()
        self.assertEqual(self.marionette.title, "We Arrive Here")

    def test_should_submit_a_form_when_any_input_element_within_that_form_is_submitted(self):
        test_html = self.marionette.absolute_url("formPage.html")
        self.marionette.navigate(test_html)
        self.marionette.find_element("id", "checky").submit()
        for i in range(5):
            try:
                self.marionette.find_element('id', 'email')
            except NoSuchElementException:
                time.sleep(1)
        self.assertEqual(self.marionette.title, "We Arrive Here")

    def test_should_submit_a_form_when_any_element_wihin_that_form_is_submitted(self):
        test_html = self.marionette.absolute_url("formPage.html")
        self.marionette.navigate(test_html)
        self.marionette.find_element("xpath", "//form/p").submit()
        for i in range(5):
            try:
                self.marionette.find_element('id', 'email')
            except NoSuchElementException:
                time.sleep(1)
        self.assertEqual(self.marionette.title, "We Arrive Here")
