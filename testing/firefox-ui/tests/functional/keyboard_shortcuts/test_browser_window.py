# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from firefox_puppeteer import PuppeteerMixin
from marionette import MarionetteTestCase
from marionette_driver import Wait


class TestBrowserWindowShortcuts(PuppeteerMixin, MarionetteTestCase):

    def test_addons_manager(self):
        # If an about:xyz page is visible, no new tab will be opened
        with self.marionette.using_context('content'):
            self.marionette.navigate('about:')

        # TODO: To be moved to the upcoming add-ons library
        def opener(tab):
            tab.window.send_shortcut(tab.window.get_entity('addons.commandkey'),
                                     accel=True, shift=True)
        self.browser.tabbar.open_tab(opener)

        # TODO: Marionette currently fails to detect the correct tab
        # with self.marionette.using_content('content'):
        #     self.wait_for_condition(lambda mn: mn.get_url() == "about:addons")

        # TODO: remove extra switch once it is done automatically
        self.browser.tabbar.tabs[1].switch_to()
        self.browser.tabbar.close_tab()

    def test_search_field(self):
        current_name = self.marionette.execute_script("""
            return window.document.activeElement.localName;
        """)

        # This doesn't test anything if we're already at input.
        self.assertNotEqual(current_name, "input")

        # TODO: To be moved to the upcoming search library
        if self.puppeteer.platform == 'linux':
            key = 'searchFocusUnix.commandkey'
        else:
            key = 'searchFocus.commandkey'
        self.browser.send_shortcut(self.browser.get_entity(key), accel=True)

        # TODO: Check that the right input box is focused
        # Located below searchbar as class="autocomplete-textbox textbox-input"
        # Anon locator has not been released yet (bug 1080764)
        def has_input_selected(mn):
            selection_name = mn.execute_script("""
                return window.document.activeElement.localName;
            """)
            return selection_name == "input"

        Wait(self.marionette).until(has_input_selected)
