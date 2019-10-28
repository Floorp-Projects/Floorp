# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
from firefox_puppeteer import PuppeteerMixin
from marionette_driver import expected, By, Wait
from marionette_harness import MarionetteTestCase


class TestNavBar(PuppeteerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestNavBar, self).setUp()

        self.navbar = self.browser.navbar
        self.url = self.marionette.absolute_url('layout/mozilla.html')

        with self.marionette.using_context('content'):
            self.marionette.navigate('about:blank')

        # TODO: check why self.puppeteer.places.remove_all_history() does not work here
        self.marionette.execute_script("""
            let count = gBrowser.sessionHistory.count;
            gBrowser.sessionHistory.legacySHistory.PurgeHistory(count);
        """)

    def test_elements(self):
        self.assertEqual(self.navbar.back_button.get_property('localName'), 'toolbarbutton')
        self.assertEqual(self.navbar.forward_button.get_property('localName'), 'toolbarbutton')
        self.assertEqual(self.navbar.home_button.get_property('localName'), 'toolbarbutton')
        self.assertEqual(self.navbar.menu_button.get_property('localName'), 'toolbarbutton')
        self.assertEqual(self.navbar.toolbar.get_property('localName'), 'toolbar')

    def test_buttons(self):
        self.marionette.set_context('content')

        # Load initial web page
        self.marionette.navigate(self.url)
        Wait(self.marionette).until(expected.element_present(lambda m:
                                    m.find_element(By.ID, 'mozilla_logo')))

        with self.marionette.using_context('chrome'):
            # Both buttons are disabled
            self.assertFalse(self.navbar.back_button.is_enabled())
            self.assertFalse(self.navbar.forward_button.is_enabled())

            # Go to the homepage
            self.navbar.home_button.click()

        Wait(self.marionette).until(expected.element_not_present(lambda m:
                                    m.find_element(By.ID, 'mozilla_logo')))
        self.assertEqual(self.marionette.get_url(), self.browser.default_homepage)

        with self.marionette.using_context('chrome'):
            # Only back button is enabled
            self.assertTrue(self.navbar.back_button.is_enabled())
            self.assertFalse(self.navbar.forward_button.is_enabled())

            # Navigate back
            self.navbar.back_button.click()

        Wait(self.marionette).until(expected.element_present(lambda m:
                                    m.find_element(By.ID, 'mozilla_logo')))
        self.assertEqual(self.marionette.get_url(), self.url)

        with self.marionette.using_context('chrome'):
            # Only forward button is enabled
            self.assertFalse(self.navbar.back_button.is_enabled())
            self.assertTrue(self.navbar.forward_button.is_enabled())

            # Navigate forward
            self.navbar.forward_button.click()

        Wait(self.marionette).until(expected.element_not_present(lambda m:
                                    m.find_element(By.ID, 'mozilla_logo')))
        self.assertEqual(self.marionette.get_url(), self.browser.default_homepage)


class TestLocationBar(PuppeteerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestLocationBar, self).setUp()

        self.locationbar = self.browser.navbar.locationbar

    def test_elements(self):
        self.assertEqual(self.locationbar.urlbar_input.get_property('id'), 'urlbar-input')

        self.assertEqual(self.locationbar.reload_button.get_property('localName'),
                         'toolbarbutton')
        self.assertEqual(self.locationbar.stop_button.get_property('localName'),
                         'toolbarbutton')

    def test_reload(self):
        event_types = ["shortcut", "shortcut2", "button"]
        for event in event_types:
            # TODO: Until we have waitForPageLoad, this only tests API
            # compatibility.
            self.locationbar.reload_url(event, force=True)
            self.locationbar.reload_url(event, force=False)

    def test_focus_and_clear(self):
        self.locationbar.urlbar.send_keys("zyx")
        self.locationbar.clear()
        self.assertEqual(self.locationbar.value, '')

        self.locationbar.urlbar.send_keys("zyx")
        self.assertEqual(self.locationbar.value, 'zyx')

        self.locationbar.clear()
        self.assertEqual(self.locationbar.value, '')

    def test_load_url(self):
        data_uri = 'data:text/html,<title>Title</title>'
        self.locationbar.load_url(data_uri)

        with self.marionette.using_context('content'):
            Wait(self.marionette).until(lambda mn: mn.get_url() == data_uri)
