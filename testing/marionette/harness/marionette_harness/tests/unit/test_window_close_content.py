# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_harness import MarionetteTestCase, skip_if_mobile, WindowManagerMixin


class TestCloseWindow(WindowManagerMixin, MarionetteTestCase):

    def tearDown(self):
        self.close_all_windows()
        self.close_all_tabs()

        super(TestCloseWindow, self).tearDown()

    @skip_if_mobile("Interacting with chrome windows not available for Fennec")
    def test_close_chrome_window_for_browser_window(self):
        win = self.open_window()
        self.marionette.switch_to_window(win)

        self.assertNotIn(win, self.marionette.window_handles)
        chrome_window_handles = self.marionette.close_chrome_window()
        self.assertNotIn(win, chrome_window_handles)
        self.assertListEqual(self.start_windows, chrome_window_handles)
        self.assertNotIn(win, self.marionette.window_handles)

    @skip_if_mobile("Interacting with chrome windows not available for Fennec")
    def test_close_chrome_window_for_non_browser_window(self):

        def open_window_with_js():
            with self.marionette.using_context("chrome"):
                self.marionette.execute_script("""
                  window.open('chrome://marionette/content/test.xul',
                              'foo', 'chrome,centerscreen');
                """)

        win = self.open_window(trigger=open_window_with_js)
        self.marionette.switch_to_window(win)

        self.assertIn(win, self.marionette.chrome_window_handles)
        self.assertNotIn(win, self.marionette.window_handles)
        chrome_window_handles = self.marionette.close_chrome_window()
        self.assertNotIn(win, chrome_window_handles)
        self.assertListEqual(self.start_windows, chrome_window_handles)
        self.assertNotIn(win, self.marionette.window_handles)

    @skip_if_mobile("Interacting with chrome windows not available for Fennec")
    def test_close_chrome_window_for_last_open_window(self):
        self.close_all_windows()

        self.assertListEqual([], self.marionette.close_chrome_window())
        self.assertListEqual([self.start_tab], self.marionette.window_handles)
        self.assertListEqual([self.start_window], self.marionette.chrome_window_handles)
        self.assertIsNotNone(self.marionette.session)

    @skip_if_mobile("Needs application independent method to open a new tab")
    def test_close_window_for_browser_tab(self):
        tab = self.open_tab()
        self.marionette.switch_to_window(tab)

        window_handles = self.marionette.close()
        self.assertNotIn(tab, window_handles)
        self.assertListEqual(self.start_tabs, window_handles)

    @skip_if_mobile("Interacting with chrome windows not available for Fennec")
    def test_close_window_for_browser_window_with_single_tab(self):
        win = self.open_window()
        self.marionette.switch_to_window(win)

        self.assertEqual(len(self.start_tabs) + 1, len(self.marionette.window_handles))
        window_handles = self.marionette.close()
        self.assertNotIn(win, window_handles)
        self.assertListEqual(self.start_tabs, window_handles)
        self.assertListEqual(self.start_windows, self.marionette.chrome_window_handles)

    def test_close_window_for_last_open_tab(self):
        self.close_all_tabs()

        self.assertListEqual([], self.marionette.close())
        self.assertListEqual([self.start_tab], self.marionette.window_handles)
        self.assertListEqual([self.start_window], self.marionette.chrome_window_handles)
        self.assertIsNotNone(self.marionette.session)

    @skip_if_mobile("discardBrowser is only available in Firefox")
    def test_close_browserless_tab(self):
        self.close_all_tabs()

        test_page = self.marionette.absolute_url("windowHandles.html")
        tab = self.open_tab()
        self.marionette.switch_to_window(tab)
        self.marionette.navigate(test_page)
        self.marionette.switch_to_window(self.start_tab)

        with self.marionette.using_context("chrome"):
            self.marionette.execute_async_script("""
              Components.utils.import("resource:///modules/BrowserWindowTracker.jsm");

              let win = BrowserWindowTracker.getTopWindow();
              win.addEventListener("TabBrowserDiscarded", ev => {
                marionetteScriptFinished(true);
              }, { once: true});
              win.gBrowser.discardBrowser(win.gBrowser.tabs[1].linkedBrowser);
            """)

        window_handles = self.marionette.window_handles
        window_handles.remove(self.start_tab)
        self.assertEqual(1, len(window_handles))
        self.marionette.switch_to_window(window_handles[0], focus=False)
        self.marionette.close()
        self.assertListEqual([self.start_tab], self.marionette.window_handles)
