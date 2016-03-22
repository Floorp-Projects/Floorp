# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver import By, Wait
from marionette_driver.errors import NoSuchWindowException, TimeoutException

import firefox_puppeteer.errors as errors

from firefox_puppeteer.ui.windows import BaseWindow
from firefox_ui_harness.testcases import FirefoxTestCase


class TestWindows(FirefoxTestCase):

    def tearDown(self):
        try:
            self.windows.close_all([self.browser])
        finally:
            FirefoxTestCase.tearDown(self)

    def test_windows(self):
        url = self.marionette.absolute_url('layout/mozilla.html')

        # Open two more windows
        for index in range(0, 2):
            self.marionette.execute_script(""" window.open(); """)

        windows = self.windows.all
        self.assertEquals(len(windows), 3)

        # Switch to the 2nd window
        self.windows.switch_to(windows[1].handle)
        self.assertEquals(windows[1].handle, self.marionette.current_chrome_window_handle)

        # TODO: Needs updated tabs module for improved navigation
        with self.marionette.using_context('content'):
            self.marionette.navigate(url)

        # Switch to the last window and find 2nd window by URL
        self.windows.switch_to(windows[2].handle)

        # TODO: A window can have multiple tabs, so this may need an update
        # when the tabs module gets implemented
        def find_by_url(win):
            with win.marionette.using_context('content'):
                return win.marionette.get_url() == url

        self.windows.switch_to(find_by_url)
        self.assertEquals(windows[1].handle, self.marionette.current_chrome_window_handle)

        self.windows.switch_to(find_by_url)

        # Switching to an unknown handles has to fail
        self.assertRaises(NoSuchWindowException,
                          self.windows.switch_to, "humbug")
        self.assertRaises(NoSuchWindowException,
                          self.windows.switch_to, lambda win: False)

        self.windows.close_all([self.browser])
        self.browser.switch_to()

        self.assertEqual(len(self.windows.all), 1)


class TestBaseWindow(FirefoxTestCase):

    def tearDown(self):
        try:
            self.windows.close_all([self.browser])
        finally:
            FirefoxTestCase.tearDown(self)

    def test_basics(self):
        # force BaseWindow instance
        win1 = BaseWindow(lambda: self.marionette, self.browser.handle)

        self.assertEquals(win1.handle, self.marionette.current_chrome_window_handle)
        self.assertEquals(win1.window_element,
                          self.marionette.find_element(By.CSS_SELECTOR, ':root'))
        self.assertEquals(win1.window_element.get_attribute('windowtype'),
                          self.marionette.get_window_type())
        self.assertFalse(win1.closed)

        # Test invalid parameters for BaseWindow constructor
        self.assertRaises(TypeError,
                          BaseWindow, self.marionette, self.browser.handle)
        self.assertRaises(errors.UnknownWindowError,
                          BaseWindow, lambda: self.marionette, 10)

        # Test invalid shortcuts
        self.assertRaises(KeyError,
                          win1.send_shortcut, 'l', acel=True)

    def test_open_close(self):
        # force BaseWindow instance
        win1 = BaseWindow(lambda: self.marionette, self.browser.handle)

        # Open a new window (will be focused), and check states
        win2 = win1.open_window()

        # force BaseWindow instance
        win2 = BaseWindow(lambda: self.marionette, win2.handle)

        self.assertEquals(len(self.marionette.chrome_window_handles), 2)
        self.assertNotEquals(win1.handle, win2.handle)
        self.assertEquals(win2.handle, self.marionette.current_chrome_window_handle)

        win2.close()

        self.assertTrue(win2.closed)
        self.assertEquals(len(self.marionette.chrome_window_handles), 1)
        self.assertEquals(win2.handle, self.marionette.current_chrome_window_handle)
        Wait(self.marionette).until(lambda _: win1.focused)  # catch the no focused window

        win1.focus()

        # Open and close a new window by a custom callback
        def opener(window):
            window.marionette.execute_script(""" window.open(); """)

        def closer(window):
            window.marionette.execute_script(""" window.close(); """)

        win2 = win1.open_window(callback=opener)

        # force BaseWindow instance
        win2 = BaseWindow(lambda: self.marionette, win2.handle)

        self.assertEquals(len(self.marionette.chrome_window_handles), 2)
        win2.close(callback=closer)

        win1.focus()

        # Check for an unexpected window class
        self.assertRaises(errors.UnexpectedWindowTypeError,
                          win1.open_window, expected_window_class=BaseWindow)
        self.windows.close_all([win1])

    def test_switch_to_and_focus(self):
        # force BaseWindow instance
        win1 = BaseWindow(lambda: self.marionette, self.browser.handle)

        # Open a new window (will be focused), and check states
        win2 = win1.open_window()

        # force BaseWindow instance
        win2 = BaseWindow(lambda: self.marionette, win2.handle)

        self.assertEquals(win2.handle, self.marionette.current_chrome_window_handle)
        self.assertEquals(win2.handle, self.windows.focused_chrome_window_handle)
        self.assertFalse(win1.focused)
        self.assertTrue(win2.focused)

        # Switch back to win1 without moving the focus, but focus separately
        win1.switch_to()
        self.assertEquals(win1.handle, self.marionette.current_chrome_window_handle)
        self.assertTrue(win2.focused)

        win1.focus()
        self.assertTrue(win1.focused)

        # Switch back to win2 by focusing it directly
        win2.focus()
        self.assertEquals(win2.handle, self.marionette.current_chrome_window_handle)
        self.assertEquals(win2.handle, self.windows.focused_chrome_window_handle)
        self.assertTrue(win2.focused)

        # Close win2, and check that it keeps active but looses focus
        win2.switch_to()
        win2.close()

        win1.switch_to()


class TestBrowserWindow(FirefoxTestCase):

    def tearDown(self):
        try:
            self.windows.close_all([self.browser])
        finally:
            FirefoxTestCase.tearDown(self)

    def test_basic(self):
        self.assertNotEqual(self.browser.dtds, [])
        self.assertNotEqual(self.browser.properties, [])

        self.assertFalse(self.browser.is_private)

        self.assertIsNotNone(self.browser.menubar)
        self.assertIsNotNone(self.browser.navbar)
        self.assertIsNotNone(self.browser.tabbar)

    def test_open_close(self):
        # open and close a new browser windows by menu
        win2 = self.browser.open_browser(trigger='menu')
        self.assertEquals(win2, self.windows.current)
        self.assertFalse(self.browser.is_private)
        win2.close(trigger='menu')

        # open and close a new browser window by shortcut
        win2 = self.browser.open_browser(trigger='shortcut')
        self.assertEquals(win2, self.windows.current)
        self.assertFalse(self.browser.is_private)
        win2.close(trigger='shortcut')

        # open and close a new private browsing window
        win2 = self.browser.open_browser(is_private=True)
        self.assertEquals(win2, self.windows.current)
        self.assertTrue(win2.is_private)
        win2.close()

        # open and close a new private browsing window
        win2 = self.browser.open_browser(trigger='shortcut', is_private=True)
        self.assertEquals(win2, self.windows.current)
        self.assertTrue(win2.is_private)
        win2.close()

        # force closing a window
        win2 = self.browser.open_browser()
        self.assertEquals(win2, self.windows.current)
        win2.close(force=True)
