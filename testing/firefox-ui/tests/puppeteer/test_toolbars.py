# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from firefox_puppeteer import PuppeteerMixin
from marionette import MarionetteTestCase
from marionette_driver import expected, By, Wait
from marionette_driver.errors import NoSuchElementException


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
            gBrowser.sessionHistory.PurgeHistory(count);
        """)

    def test_elements(self):
        self.assertEqual(self.navbar.back_button.get_attribute('localName'), 'toolbarbutton')
        self.assertEqual(self.navbar.forward_button.get_attribute('localName'), 'toolbarbutton')
        self.assertEqual(self.navbar.home_button.get_attribute('localName'), 'toolbarbutton')
        self.assertEqual(self.navbar.menu_button.get_attribute('localName'), 'toolbarbutton')
        self.assertEqual(self.navbar.toolbar.get_attribute('localName'), 'toolbar')

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
        self.assertEqual(self.locationbar.urlbar.get_attribute('localName'), 'textbox')
        self.assertIn('urlbar-input', self.locationbar.urlbar_input.get_attribute('className'))

        self.assertEqual(self.locationbar.connection_icon.get_attribute('localName'), 'image')
        self.assertEqual(self.locationbar.identity_box.get_attribute('localName'), 'box')
        self.assertEqual(self.locationbar.identity_country_label.get_attribute('localName'),
                         'label')
        self.assertEqual(self.locationbar.identity_organization_label.get_attribute('localName'),
                         'label')
        self.assertEqual(self.locationbar.identity_icon.get_attribute('localName'), 'image')
        self.assertEqual(self.locationbar.history_drop_marker.get_attribute('localName'),
                         'dropmarker')
        self.assertEqual(self.locationbar.reload_button.get_attribute('localName'),
                         'toolbarbutton')
        self.assertEqual(self.locationbar.stop_button.get_attribute('localName'),
                         'toolbarbutton')

        self.assertEqual(self.locationbar.contextmenu.get_attribute('localName'), 'menupopup')
        self.assertEqual(self.locationbar.get_contextmenu_entry('paste').get_attribute('cmd'),
                         'cmd_paste')

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


class TestAutoCompleteResults(PuppeteerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestAutoCompleteResults, self).setUp()

        self.browser.navbar.locationbar.clear()

        self.autocomplete_results = self.browser.navbar.locationbar.autocomplete_results

    def tearDown(self):
        try:
            self.autocomplete_results.close(force=True)
        except NoSuchElementException:
            # TODO: A NoSuchElementException is thrown here when tests accessing the
            # autocomplete_results element are skipped.
            pass
        finally:
            super(TestAutoCompleteResults, self).tearDown()

    def test_popup_elements(self):
        # TODO: This test is not very robust because it relies on the history
        # in the default profile.
        self.assertFalse(self.autocomplete_results.is_open)
        self.browser.navbar.locationbar.urlbar.send_keys('a')
        results = self.autocomplete_results.results
        Wait(self.marionette).until(lambda _: self.autocomplete_results.is_complete)
        visible_result_count = len(self.autocomplete_results.visible_results)
        self.assertTrue(visible_result_count > 0)
        self.assertEqual(visible_result_count,
                         int(results.get_attribute('itemCount')))

    def test_close(self):
        self.browser.navbar.locationbar.urlbar.send_keys('a')
        Wait(self.marionette).until(lambda _: self.autocomplete_results.is_open)
        # The Wait in the library implementation will fail this if this doesn't
        # end up closing.
        self.autocomplete_results.close()

    def test_force_close(self):
        self.browser.navbar.locationbar.urlbar.send_keys('a')
        Wait(self.marionette).until(lambda _: self.autocomplete_results.is_open)
        # The Wait in the library implementation will fail this if this doesn't
        # end up closing.
        self.autocomplete_results.close(force=True)

    def test_matching_text(self):
        # The default profile always has existing bookmarks. So no sites have to
        # be visited and bookmarked.
        for input_text in ('about', 'zilla'):
            self.browser.navbar.locationbar.urlbar.clear()
            self.browser.navbar.locationbar.urlbar.send_keys(input_text)
            Wait(self.marionette).until(lambda _: self.autocomplete_results.is_open)
            Wait(self.marionette).until(lambda _: self.autocomplete_results.is_complete)
            visible_results = self.autocomplete_results.visible_results
            self.assertTrue(len(visible_results) > 0)

            for result in visible_results:
                # check matching text only for results of type bookmark
                if result.get_attribute('type') != 'bookmark':
                    continue
                title_matches = self.autocomplete_results.get_matching_text(result, "title")
                url_matches = self.autocomplete_results.get_matching_text(result, "url")
                all_matches = title_matches + url_matches
                self.assertTrue(len(all_matches) > 0)
                for match_fragment in all_matches:
                    self.assertIn(match_fragment.lower(), input_text)

            self.autocomplete_results.close()


class TestIdentityPopup(PuppeteerMixin, MarionetteTestCase):
    def setUp(self):
        super(TestIdentityPopup, self).setUp()

        self.locationbar = self.browser.navbar.locationbar
        self.identity_popup = self.locationbar.identity_popup

        self.url = 'https://ssl-ev.mozqa.com'

        with self.marionette.using_context('content'):
            self.marionette.navigate(self.url)

    def tearDown(self):
        try:
            self.identity_popup.close(force=True)
        finally:
            super(TestIdentityPopup, self).tearDown()

    def test_elements(self):
        self.locationbar.open_identity_popup()

        # Test main view elements
        main = self.identity_popup.view.main
        self.assertEqual(main.element.get_attribute('localName'), 'panelview')

        self.assertEqual(main.expander.get_attribute('localName'), 'button')
        self.assertEqual(main.host.get_attribute('localName'), 'label')
        self.assertEqual(main.insecure_connection_label.get_attribute('localName'),
                         'description')
        self.assertEqual(main.internal_connection_label.get_attribute('localName'),
                         'description')
        self.assertEqual(main.secure_connection_label.get_attribute('localName'),
                         'description')

        self.assertEqual(main.permissions.get_attribute('localName'), 'vbox')

        # Test security view elements
        security = self.identity_popup.view.security
        self.assertEqual(security.element.get_attribute('localName'), 'panelview')

        self.assertEqual(security.host.get_attribute('localName'), 'label')
        self.assertEqual(security.insecure_connection_label.get_attribute('localName'),
                         'description')
        self.assertEqual(security.secure_connection_label.get_attribute('localName'),
                         'description')

        self.assertEqual(security.owner.get_attribute('localName'), 'description')
        self.assertEqual(security.owner_location.get_attribute('localName'), 'description')
        self.assertEqual(security.verifier.get_attribute('localName'), 'description')

        self.assertEqual(security.disable_mixed_content_blocking_button.get_attribute('localName'),
                         'button')
        self.assertEqual(security.enable_mixed_content_blocking_button.get_attribute('localName'),
                         'button')

        self.assertEqual(security.more_info_button.get_attribute('localName'), 'button')

    def test_open_close(self):
        with self.marionette.using_context('content'):
            self.marionette.navigate(self.url)

        self.assertFalse(self.identity_popup.is_open)

        self.locationbar.open_identity_popup()

        self.identity_popup.close()
        self.assertFalse(self.identity_popup.is_open)

    def test_force_close(self):
        with self.marionette.using_context('content'):
            self.marionette.navigate(self.url)

        self.assertFalse(self.identity_popup.is_open)

        self.locationbar.open_identity_popup()

        self.identity_popup.close(force=True)
        self.assertFalse(self.identity_popup.is_open)
