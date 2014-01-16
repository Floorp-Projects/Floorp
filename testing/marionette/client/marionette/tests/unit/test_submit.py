# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import time

from by import By
from errors import NoSuchElementException
from marionette_test import MarionetteTestCase
from wait import Wait


class TestSubmit(MarionetteTestCase):

    def test_should_be_able_to_submit_forms(self):
        test_html = self.marionette.absolute_url("formPage.html")
        self.marionette.navigate(test_html)
        self.marionette.find_element(By.NAME, "login").submit()
        for x in xrange(1, 10):
            try:
                self.marionette.find_element(By.NAME, "login")
                time.sleep(1)
            except NoSuchElementException:
                break

        Wait(self.marionette, timeout=10, ignored_exceptions=NoSuchElementException)\
            .until(lambda m: m.find_element(By.ID, 'email'))
        title = self.marionette.title
        count = 0
        while self.marionette.title == '' and count <= 10:
            time.sleep(1)
            title = self.marionette.title
            count = count + 1
            if title != '':
                break
        self.assertEqual(title, "We Arrive Here")

    def test_should_submit_a_form_when_any_input_element_within_that_form_is_submitted(self):
        test_html = self.marionette.absolute_url("formPage.html")
        self.marionette.navigate(test_html)
        self.marionette.find_element(By.ID, "checky").submit()

        for x in xrange(1, 10):
            try:
                self.marionette.find_element(By.ID, "checky")
                time.sleep(1)
            except NoSuchElementException:
                break

        Wait(self.marionette, ignored_exceptions=NoSuchElementException).until(
            lambda m: m.find_element(By.ID, 'email'))
        self.assertEqual(self.marionette.title, "We Arrive Here")

    def test_should_submit_a_form_when_any_element_wihin_that_form_is_submitted(self):
        test_html = self.marionette.absolute_url("formPage.html")
        self.marionette.navigate(test_html)
        self.marionette.find_element(By.XPATH, "//form/p").submit()

        for x in xrange(1, 10):
            try:
                self.marionette.find_element(By.XPATH, "//form/p")
                time.sleep(1)
            except NoSuchElementException:
                break

        Wait(self.marionette, ignored_exceptions=NoSuchElementException).until(
            lambda m: m.find_element(By.ID, 'email'))
        self.assertEqual(self.marionette.title, "We Arrive Here")
