# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
from firefox_puppeteer import PuppeteerMixin
from marionette_driver import Wait
from marionette_harness import MarionetteTestCase


class TestBrowserWindowShortcuts(PuppeteerMixin, MarionetteTestCase):

    def test_addons_manager(self):
        # If an about:xyz page is visible, no new tab will be opened
        with self.marionette.using_context('content'):
            self.marionette.navigate('about:about')

        # TODO: To be moved to the upcoming add-ons library
        def opener(tab):
            tab.window.send_shortcut('a', accel=True, shift=True)
        self.browser.tabbar.open_tab(opener)

        # TODO: Marionette currently fails to detect the correct tab
        # with self.marionette.using_content('content'):
        #     Wait(self.marionette).until(
        #         lambda mn: mn.get_url() == "about:addons",
        #         message="Url does not match with 'about:addons'"
        #     )

        # TODO: remove extra switch once it is done automatically
        self.browser.tabbar.tabs[1].switch_to()
        self.browser.tabbar.close_tab()

    def test_search_field(self):
        current_name = self.marionette.execute_script("""
            return window.document.activeElement.localName;
        """)

        # This doesn't test anything if we're already at input.
        self.assertNotEqual(current_name, "input")

        is_linux = self.marionette.session_capabilities['platformName'] == 'linux'
        self.browser.send_shortcut('j' if is_linux else 'k',
                                   accel=True)

        # TODO: Check that the right input box is focused
        # Located below searchbar as class="textbox-input"
        def has_input_selected(mn):
            selection_name = mn.execute_script("""
                return window.document.activeElement.localName;
            """)
            return selection_name == "input"

        Wait(self.marionette).until(has_input_selected)
