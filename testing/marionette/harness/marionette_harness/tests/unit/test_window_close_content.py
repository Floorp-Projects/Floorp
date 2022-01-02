# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from six.moves.urllib.parse import quote

from marionette_driver.by import By
from marionette_harness import MarionetteTestCase, WindowManagerMixin


def inline(doc):
    return "data:text/html;charset=utf-8,{}".format(quote(doc))


class TestCloseWindow(WindowManagerMixin, MarionetteTestCase):
    def tearDown(self):
        self.close_all_windows()
        self.close_all_tabs()

        super(TestCloseWindow, self).tearDown()

    def test_close_chrome_window_for_browser_window(self):
        with self.marionette.using_context("chrome"):
            new_window = self.open_window()
        self.marionette.switch_to_window(new_window)

        self.assertIn(new_window, self.marionette.chrome_window_handles)
        chrome_window_handles = self.marionette.close_chrome_window()
        self.assertNotIn(new_window, chrome_window_handles)
        self.assertListEqual(self.start_windows, chrome_window_handles)
        self.assertNotIn(new_window, self.marionette.window_handles)

    def test_close_chrome_window_for_non_browser_window(self):
        new_window = self.open_chrome_window(
            "chrome://remote/content/marionette/test.xhtml"
        )
        self.marionette.switch_to_window(new_window)

        self.assertIn(new_window, self.marionette.chrome_window_handles)
        self.assertNotIn(new_window, self.marionette.window_handles)
        chrome_window_handles = self.marionette.close_chrome_window()
        self.assertNotIn(new_window, chrome_window_handles)
        self.assertListEqual(self.start_windows, chrome_window_handles)
        self.assertNotIn(new_window, self.marionette.window_handles)

    def test_close_chrome_window_for_last_open_window(self):
        self.close_all_windows()

        self.assertListEqual([], self.marionette.close_chrome_window())
        self.assertListEqual([self.start_tab], self.marionette.window_handles)
        self.assertListEqual([self.start_window], self.marionette.chrome_window_handles)
        self.assertIsNotNone(self.marionette.session)

    def test_close_window_for_browser_tab(self):
        new_tab = self.open_tab()
        self.marionette.switch_to_window(new_tab)

        window_handles = self.marionette.close()
        self.assertNotIn(new_tab, window_handles)
        self.assertListEqual(self.start_tabs, window_handles)

    def test_close_window_with_dismissed_beforeunload_prompt(self):
        new_tab = self.open_tab()
        self.marionette.switch_to_window(new_tab)

        self.marionette.navigate(
            inline(
                """
          <input type="text">
          <script>
            window.addEventListener("beforeunload", function (event) {
              event.preventDefault();
            });
          </script>
        """
            )
        )

        self.marionette.find_element(By.TAG_NAME, "input").send_keys("foo")
        self.marionette.close()

    def test_close_window_for_browser_window_with_single_tab(self):
        new_tab = self.open_window()
        self.marionette.switch_to_window(new_tab)

        self.assertEqual(len(self.marionette.window_handles), len(self.start_tabs) + 1)
        window_handles = self.marionette.close()
        self.assertNotIn(new_tab, window_handles)
        self.assertListEqual(self.start_tabs, window_handles)
        self.assertListEqual(self.start_windows, self.marionette.chrome_window_handles)

    def test_close_window_for_last_open_tab(self):
        self.close_all_tabs()

        self.assertListEqual([], self.marionette.close())
        self.assertListEqual([self.start_tab], self.marionette.window_handles)
        self.assertListEqual([self.start_window], self.marionette.chrome_window_handles)
        self.assertIsNotNone(self.marionette.session)

    def test_close_browserless_tab(self):
        self.close_all_tabs()

        test_page = self.marionette.absolute_url("windowHandles.html")
        new_tab = self.open_tab()
        self.marionette.switch_to_window(new_tab)
        self.marionette.navigate(test_page)
        self.marionette.switch_to_window(self.start_tab)

        with self.marionette.using_context("chrome"):
            self.marionette.execute_async_script(
                """
              Components.utils.import("resource:///modules/BrowserWindowTracker.jsm");

              let win = BrowserWindowTracker.getTopWindow();
              win.addEventListener("TabBrowserDiscarded", ev => {
                arguments[0](true);
              }, { once: true});
              win.gBrowser.discardBrowser(win.gBrowser.tabs[1]);
            """
            )

        window_handles = self.marionette.window_handles
        window_handles.remove(self.start_tab)
        self.assertEqual(1, len(window_handles))
        self.marionette.switch_to_window(window_handles[0], focus=False)
        self.marionette.close()
        self.assertListEqual([self.start_tab], self.marionette.window_handles)
