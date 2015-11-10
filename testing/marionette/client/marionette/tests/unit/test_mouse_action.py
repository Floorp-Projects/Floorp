# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase
from marionette_driver.marionette import Actions
from marionette_driver.keys import Keys
from marionette_driver.by import By

class TestMouseAction(MarionetteTestCase):

    def setUp(self):
        MarionetteTestCase.setUp(self)
        if self.marionette.session_capabilities['platformName'] == 'Darwin':
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
        test_html = self.marionette.absolute_url("double_click.html")
        self.marionette.navigate(test_html)
        el = self.marionette.find_element(By.ID, 'one-word-div')
        self.action.double_click(el).perform()
        el.send_keys(self.mod_key + 'c')
        rel = self.marionette.find_element("id", "input-field")
        rel.send_keys(self.mod_key + 'v')
        self.assertEqual(rel.get_attribute('value'), 'zyxw')

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

    def test_chrome_click(self):
        self.marionette.navigate("about:blank")
        data_uri = "data:text/html,<html></html>"
        with self.marionette.using_context('chrome'):
            urlbar = self.marionette.find_element(By.ID, "urlbar")
            urlbar.send_keys(data_uri)
            go_button = self.marionette.find_element(By.ID, "urlbar-go-button")
            self.action.click(go_button).perform()
        self.wait_for_condition(lambda mn: mn.get_url() == data_uri)

    def test_chrome_double_click(self):
        self.marionette.navigate("about:blank")
        test_word = "quux"
        with self.marionette.using_context('chrome'):
            urlbar = self.marionette.find_element(By.ID, "urlbar")
            self.assertEqual(urlbar.get_attribute('value'), '')

            urlbar.send_keys(test_word)
            self.assertEqual(urlbar.get_attribute('value'), test_word)
            (self.action.double_click(urlbar).perform()
                        .key_down(self.mod_key)
                        .key_down('x').perform())
            self.assertEqual(urlbar.get_attribute('value'), '')

    def test_chrome_context_click_action(self):
        self.marionette.set_context('chrome')
        def context_menu_state():
            cm_el = self.marionette.find_element(By.ID, 'tabContextMenu')
            return cm_el.get_attribute('state')

        currtab = self.marionette.execute_script("return gBrowser.selectedTab")
        self.assertEqual('closed', context_menu_state())
        self.action.context_click(currtab).perform()
        self.wait_for_condition(lambda _: context_menu_state() == 'open')

        (self.marionette.find_element(By.ID, 'main-window')
                        .send_keys(Keys.ESCAPE))

        self.wait_for_condition(lambda _: context_menu_state() == 'closed')
