# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver.by import By
from marionette_driver.errors import NoSuchElementException, ElementNotVisibleException
from marionette import MarionetteTestCase
from marionette_driver.marionette import Actions
from marionette_driver.keys import Keys
from marionette_driver.wait import Wait


class TestClick(MarionetteTestCase):
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

class TestClickAction(MarionetteTestCase):

    def setUp(self):
        MarionetteTestCase.setUp(self)
        if self.marionette.session_capabilities['platformName'] == 'DARWIN':
            self.mod_key = Keys.META
        else:
            self.mod_key = Keys.CONTROL
        self.action = Actions(self.marionette)

    def test_click_action(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        link = self.marionette.find_element(By.ID, "mozLink")
        self.action.click(link).perform()
        self.assertEqual("Clicked", self.marionette.execute_script("return document.getElementById('mozLink').innerHTML;"))

    def test_clicking_element_out_of_view_succeeds(self):
        # The action based click doesn't check for visibility.
        test_html = self.marionette.absolute_url('hidden.html')
        self.marionette.navigate(test_html)
        el = self.marionette.find_element(By.ID, 'child')
        self.action.click(el).perform()

    def test_double_click_action(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)
        el = self.marionette.find_element(By.ID, 'displayed')
        # The first click just brings the element into view so text selection
        # works as expected. (A different test page could be used to isolate
        # this element and make sure it's always in view)
        el.click()
        self.action.double_click(el).perform()
        el.send_keys(self.mod_key + 'c')
        rel = self.marionette.find_element("id", "keyReporter")
        rel.send_keys(self.mod_key + 'v')
        self.assertEqual(rel.get_attribute('value'), 'Displayed')

    def test_context_click_action(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)
        click_el = self.marionette.find_element(By.ID, 'resultContainer')

        def context_menu_state():
            with self.marionette.using_context('chrome'):
                cm_el = self.marionette.find_element(By.ID, 'contentAreaContextMenu')
                return cm_el.get_attribute('state')

        self.assertEqual('closed', context_menu_state())
        self.action.context_click(click_el).perform()
        self.wait_for_condition(lambda _: context_menu_state() == 'open')

        with self.marionette.using_context('chrome'):
            (self.marionette.find_element(By.ID, 'main-window')
                            .send_keys(Keys.ESCAPE))
        self.wait_for_condition(lambda _: context_menu_state() == 'closed')

    def test_middle_click_action(self):
        test_html = self.marionette.absolute_url("clicks.html")
        self.marionette.navigate(test_html)

        self.marionette.find_element(By.ID, "addbuttonlistener").click()

        el = self.marionette.find_element(By.ID, "showbutton")
        self.action.middle_click(el).perform()

        self.wait_for_condition(
            lambda _: el.get_attribute('innerHTML') == '1')
