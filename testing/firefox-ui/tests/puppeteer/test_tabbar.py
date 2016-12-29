# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from firefox_puppeteer import PuppeteerMixin
from firefox_puppeteer.errors import NoCertificateError
from marionette_harness import MarionetteTestCase


class TestTabBar(PuppeteerMixin, MarionetteTestCase):

    def tearDown(self):
        try:
            self.browser.tabbar.close_all_tabs([self.browser.tabbar.tabs[0]])
        finally:
            super(TestTabBar, self).tearDown()

    def test_basics(self):
        tabbar = self.browser.tabbar

        self.assertEqual(tabbar.window, self.browser)

        self.assertEqual(len(tabbar.tabs), 1)
        self.assertEqual(tabbar.tabs[0].handle, self.marionette.current_window_handle)

        self.assertEqual(tabbar.newtab_button.get_property('localName'), 'toolbarbutton')
        self.assertEqual(tabbar.toolbar.get_property('localName'), 'tabs')

    def test_open_close(self):
        tabbar = self.browser.tabbar

        self.assertEqual(len(tabbar.tabs), 1)
        self.assertEqual(tabbar.selected_index, 0)

        # Open with default trigger, and force closing the tab
        tabbar.open_tab()
        tabbar.close_tab(force=True)

        # Open a new tab by each trigger method
        open_strategies = ('button',
                           'menu',
                           'shortcut',
                           lambda tab: tabbar.newtab_button.click()
                           )
        for trigger in open_strategies:
            new_tab = tabbar.open_tab(trigger=trigger)
            self.assertEqual(len(tabbar.tabs), 2)
            self.assertEqual(new_tab.handle, self.marionette.current_window_handle)
            self.assertEqual(new_tab.handle, tabbar.tabs[1].handle)

            tabbar.close_tab()
            self.assertEqual(len(tabbar.tabs), 1)
            self.assertEqual(tabbar.tabs[0].handle, self.marionette.current_window_handle)
            self.assertNotEqual(new_tab.handle, tabbar.tabs[0].handle)

        # Close a tab by each trigger method
        close_strategies = ('button',
                            'menu',
                            'shortcut',
                            lambda tab: tab.close_button.click())
        for trigger in close_strategies:
            new_tab = tabbar.open_tab()
            self.assertEqual(len(tabbar.tabs), 2)
            self.assertEqual(new_tab.handle, self.marionette.current_window_handle)
            self.assertEqual(new_tab.handle, tabbar.tabs[1].handle)

            tabbar.close_tab(trigger=trigger)
            self.assertEqual(len(tabbar.tabs), 1)
            self.assertEqual(tabbar.tabs[0].handle, self.marionette.current_window_handle)
            self.assertNotEqual(new_tab.handle, tabbar.tabs[0].handle)

    def test_close_not_selected_tab(self):
        tabbar = self.browser.tabbar

        new_tab = tabbar.open_tab()
        tabbar.close_tab(tabbar.tabs[0])

        self.assertEqual(len(tabbar.tabs), 1)
        self.assertEqual(new_tab, tabbar.tabs[0])

    def test_close_all_tabs_except_first(self):
        tabbar = self.browser.tabbar

        orig_tab = tabbar.tabs[0]

        for i in range(0, 3):
            tabbar.open_tab()

        tabbar.close_all_tabs([orig_tab])
        self.assertEqual(len(tabbar.tabs), 1)
        self.assertEqual(orig_tab.handle, self.marionette.current_window_handle)

    def test_switch_to(self):
        tabbar = self.browser.tabbar

        # Open a new tab in the foreground (will be auto-selected)
        new_tab = tabbar.open_tab()
        self.assertEqual(new_tab.handle, self.marionette.current_window_handle)
        self.assertEqual(tabbar.selected_index, 1)
        self.assertEqual(tabbar.selected_tab, new_tab)

        # Switch by index
        tabbar.switch_to(0)
        self.assertEqual(tabbar.tabs[0].handle, self.marionette.current_window_handle)

        # Switch by tab
        tabbar.switch_to(new_tab)
        self.assertEqual(new_tab.handle, self.marionette.current_window_handle)

        # Switch by callback
        tabbar.switch_to(lambda tab: tab.window.tabbar.selected_tab != tab)
        self.assertEqual(tabbar.tabs[0].handle, self.marionette.current_window_handle)

        tabbar.close_tab(tabbar.tabs[1])


class TestTab(PuppeteerMixin, MarionetteTestCase):

    def tearDown(self):
        try:
            self.browser.tabbar.close_all_tabs([self.browser.tabbar.tabs[0]])
        finally:
            super(TestTab, self).tearDown()

    def test_basic(self):
        tab = self.browser.tabbar.tabs[0]

        self.assertEqual(tab.window, self.browser)

        self.assertEqual(tab.tab_element.get_property('localName'), 'tab')
        self.assertEqual(tab.close_button.get_property('localName'), 'toolbarbutton')

    def test_certificate(self):
        url = self.marionette.absolute_url('layout/mozilla.html')

        with self.marionette.using_context(self.marionette.CONTEXT_CONTENT):
            self.marionette.navigate(url)
        with self.assertRaises(NoCertificateError):
            self.browser.tabbar.tabs[0].certificate

    def test_close(self):
        tabbar = self.browser.tabbar

        self.assertEqual(len(tabbar.tabs), 1)
        self.assertEqual(tabbar.selected_index, 0)

        # Force closing the tab
        new_tab = tabbar.open_tab()
        new_tab.close(force=True)

        # Close a tab by each trigger method
        close_strategies = ('button',
                            'menu',
                            'shortcut',
                            lambda tab: tab.close_button.click())
        for trigger in close_strategies:
            new_tab = tabbar.open_tab()
            self.assertEqual(len(tabbar.tabs), 2)
            self.assertEqual(new_tab.handle, self.marionette.current_window_handle)
            self.assertEqual(new_tab.handle, tabbar.tabs[1].handle)

            new_tab.close(trigger=trigger)
            self.assertEqual(len(tabbar.tabs), 1)
            self.assertEqual(tabbar.tabs[0].handle, self.marionette.current_window_handle)
            self.assertNotEqual(new_tab.handle, tabbar.tabs[0].handle)

    def test_location(self):
        url = self.marionette.absolute_url('layout/mozilla.html')
        with self.marionette.using_context(self.marionette.CONTEXT_CONTENT):
            self.marionette.navigate(url)
        self.assertEqual(self.browser.tabbar.tabs[0].location, url)

    def test_switch_to(self):
        tabbar = self.browser.tabbar

        new_tab = tabbar.open_tab()

        # Switch to the first tab, which will not select it
        tabbar.tabs[0].switch_to()
        self.assertEqual(tabbar.tabs[0].handle, self.marionette.current_window_handle)
        # Bug 1128656: We cannot test as long as switch_to_window() auto-selects the tab
        # self.assertEqual(tabbar.selected_index, 1)
        # self.assertEqual(tabbar.selected_tab, new_tab)

        # Now select the first tab
        tabbar.tabs[0].select()
        self.assertEqual(tabbar.tabs[0].handle, self.marionette.current_window_handle)
        self.assertTrue(tabbar.tabs[0].selected)
        self.assertFalse(tabbar.tabs[1].selected)

        new_tab.close()
