# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

from marionette_driver import By
from marionette_driver.errors import NoSuchWindowException

from marionette_harness import MarionetteTestCase, WindowManagerMixin


class TestNoSuchWindowContent(WindowManagerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestNoSuchWindowContent, self).setUp()

    def tearDown(self):
        self.close_all_windows()
        super(TestNoSuchWindowContent, self).tearDown()

    def test_closed_chrome_window(self):
        with self.marionette.using_context("chrome"):
            new_window = self.open_window()
        self.marionette.switch_to_window(new_window)
        self.marionette.close_chrome_window()

        # When closing a browser window both handles are not available
        for context in ("chrome", "content"):
            print("Testing handles with context {}".format(context))
            with self.marionette.using_context(context):
                with self.assertRaises(NoSuchWindowException):
                    self.marionette.current_chrome_window_handle
                with self.assertRaises(NoSuchWindowException):
                    self.marionette.current_window_handle

        self.marionette.switch_to_window(self.start_window)

        with self.assertRaises(NoSuchWindowException):
            self.marionette.switch_to_window(new_window)

    def test_closed_chrome_window_while_in_frame(self):
        new_window = self.open_chrome_window("chrome://marionette/content/test.xhtml")
        self.marionette.switch_to_window(new_window)

        with self.marionette.using_context("chrome"):
            self.marionette.switch_to_frame(0)
        self.marionette.close_chrome_window()

        with self.assertRaises(NoSuchWindowException):
            self.marionette.current_window_handle
        with self.assertRaises(NoSuchWindowException):
            self.marionette.current_chrome_window_handle

        self.marionette.switch_to_window(self.start_window)

        with self.assertRaises(NoSuchWindowException):
            self.marionette.switch_to_window(new_window)

    def test_closed_tab(self):
        new_tab = self.open_tab(focus=True)
        self.marionette.switch_to_window(new_tab)
        self.marionette.close()

        # Check that only the content window is not available in both contexts
        for context in ("chrome", "content"):
            with self.marionette.using_context(context):
                with self.assertRaises(NoSuchWindowException):
                    self.marionette.current_window_handle
                self.marionette.current_chrome_window_handle

        self.marionette.switch_to_window(self.start_tab)

        with self.assertRaises(NoSuchWindowException):
            self.marionette.switch_to_window(new_tab)

    def test_closed_tab_while_in_frame(self):
        new_tab = self.open_tab()
        self.marionette.switch_to_window(new_tab)

        with self.marionette.using_context("content"):
            self.marionette.navigate(self.marionette.absolute_url("test_iframe.html"))
            frame = self.marionette.find_element(By.ID, "test_iframe")
            self.marionette.switch_to_frame(frame)

        self.marionette.close()

        with self.assertRaises(NoSuchWindowException):
            self.marionette.current_window_handle
        self.marionette.current_chrome_window_handle

        self.marionette.switch_to_window(self.start_tab)

        with self.assertRaises(NoSuchWindowException):
            self.marionette.switch_to_window(new_tab)
