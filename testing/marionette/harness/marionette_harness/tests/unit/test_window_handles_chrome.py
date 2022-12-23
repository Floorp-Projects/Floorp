# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import six

from marionette_driver import errors
from marionette_harness import MarionetteTestCase, WindowManagerMixin


class TestWindowHandles(WindowManagerMixin, MarionetteTestCase):
    def setUp(self):
        super(TestWindowHandles, self).setUp()

        self.chrome_dialog = "chrome://remote/content/marionette/test_dialog.xhtml"

        self.marionette.set_context("chrome")

    def tearDown(self):
        self.close_all_windows()
        self.close_all_tabs()

        super(TestWindowHandles, self).tearDown()

    def assert_window_handles(self):
        try:
            self.assertIsInstance(
                self.marionette.current_chrome_window_handle, six.string_types
            )
            self.assertIsInstance(
                self.marionette.current_window_handle, six.string_types
            )
        except errors.NoSuchWindowException:
            pass

        for handle in self.marionette.chrome_window_handles:
            self.assertIsInstance(handle, six.string_types)

        for handle in self.marionette.window_handles:
            self.assertIsInstance(handle, six.string_types)

    def test_chrome_window_handles_with_scopes(self):
        new_browser = self.open_window()
        self.assert_window_handles()
        self.assertEqual(
            len(self.marionette.chrome_window_handles), len(self.start_windows) + 1
        )
        self.assertIn(new_browser, self.marionette.chrome_window_handles)
        self.assertEqual(
            self.marionette.current_chrome_window_handle, self.start_window
        )

        new_dialog = self.open_chrome_window(self.chrome_dialog)
        self.assert_window_handles()
        self.assertEqual(
            len(self.marionette.chrome_window_handles), len(self.start_windows) + 2
        )
        self.assertIn(new_dialog, self.marionette.chrome_window_handles)
        self.assertEqual(
            self.marionette.current_chrome_window_handle, self.start_window
        )

        chrome_window_handles_in_chrome_scope = self.marionette.chrome_window_handles
        window_handles_in_chrome_scope = self.marionette.window_handles

        with self.marionette.using_context("content"):
            self.assertEqual(
                self.marionette.chrome_window_handles,
                chrome_window_handles_in_chrome_scope,
            )
            self.assertEqual(
                self.marionette.window_handles, window_handles_in_chrome_scope
            )

    def test_chrome_window_handles_after_opening_new_chrome_window(self):
        new_window = self.open_chrome_window(self.chrome_dialog)
        self.assert_window_handles()
        self.assertEqual(
            len(self.marionette.chrome_window_handles), len(self.start_windows) + 1
        )
        self.assertIn(new_window, self.marionette.chrome_window_handles)
        self.assertEqual(
            self.marionette.current_chrome_window_handle, self.start_window
        )

        # Check that the new chrome window has the correct URL loaded
        self.marionette.switch_to_window(new_window)
        self.assert_window_handles()
        self.assertEqual(self.marionette.current_chrome_window_handle, new_window)
        self.assertEqual(self.marionette.get_url(), self.chrome_dialog)

        # Close the chrome window, and carry on in our original window.
        self.marionette.close_chrome_window()
        self.assert_window_handles()
        self.assertEqual(
            len(self.marionette.chrome_window_handles), len(self.start_windows)
        )
        self.assertNotIn(new_window, self.marionette.chrome_window_handles)

        self.marionette.switch_to_window(self.start_window)
        self.assert_window_handles()
        self.assertEqual(
            self.marionette.current_chrome_window_handle, self.start_window
        )

    def test_chrome_window_handles_after_opening_new_window(self):
        new_window = self.open_window()
        self.assert_window_handles()
        self.assertEqual(
            len(self.marionette.chrome_window_handles), len(self.start_windows) + 1
        )
        self.assertIn(new_window, self.marionette.chrome_window_handles)
        self.assertEqual(
            self.marionette.current_chrome_window_handle, self.start_window
        )

        self.marionette.switch_to_window(new_window)
        self.assert_window_handles()
        self.assertEqual(self.marionette.current_chrome_window_handle, new_window)

        # Close the opened window and carry on in our original window.
        self.marionette.close()
        self.assert_window_handles()
        self.assertEqual(
            len(self.marionette.chrome_window_handles), len(self.start_windows)
        )
        self.assertNotIn(new_window, self.marionette.chrome_window_handles)

        self.marionette.switch_to_window(self.start_window)
        self.assert_window_handles()
        self.assertEqual(
            self.marionette.current_chrome_window_handle, self.start_window
        )

    def test_chrome_window_handles_after_session_created(self):
        new_window = self.open_chrome_window(self.chrome_dialog)
        self.assert_window_handles()
        self.assertEqual(
            len(self.marionette.chrome_window_handles), len(self.start_windows) + 1
        )
        self.assertIn(new_window, self.marionette.chrome_window_handles)
        self.assertEqual(
            self.marionette.current_chrome_window_handle, self.start_window
        )

        chrome_window_handles = self.marionette.chrome_window_handles

        self.marionette.delete_session()
        self.marionette.start_session()

        self.assert_window_handles()
        self.assertEqual(chrome_window_handles, self.marionette.chrome_window_handles)

        self.marionette.switch_to_window(new_window)

    def test_window_handles_after_opening_new_tab(self):
        with self.marionette.using_context("content"):
            new_tab = self.open_tab()
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.window_handles), len(self.start_tabs) + 1)
        self.assertIn(new_tab, self.marionette.window_handles)
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)

        self.marionette.switch_to_window(new_tab)
        self.assert_window_handles()
        self.assertEqual(self.marionette.current_window_handle, new_tab)

        self.marionette.switch_to_window(self.start_tab)
        self.assert_window_handles()
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)

        self.marionette.switch_to_window(new_tab)
        self.marionette.close()
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.window_handles), len(self.start_tabs))
        self.assertNotIn(new_tab, self.marionette.window_handles)

        self.marionette.switch_to_window(self.start_tab)
        self.assert_window_handles()
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)

    def test_window_handles_after_opening_new_foreground_tab(self):
        with self.marionette.using_context("content"):
            new_tab = self.open_tab(focus=True)
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.window_handles), len(self.start_tabs) + 1)
        self.assertIn(new_tab, self.marionette.window_handles)
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)

        # We still have the default tab set as our window handle. This
        # get_url command should be sent immediately, and not be forever-queued.
        with self.marionette.using_context("content"):
            self.marionette.get_url()

        self.marionette.switch_to_window(new_tab)
        self.assert_window_handles()
        self.assertEqual(self.marionette.current_window_handle, new_tab)

        self.marionette.close()
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.window_handles), len(self.start_tabs))
        self.assertNotIn(new_tab, self.marionette.window_handles)

        self.marionette.switch_to_window(self.start_tab)
        self.assert_window_handles()
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)

    def test_window_handles_after_opening_new_chrome_window(self):
        new_window = self.open_chrome_window(self.chrome_dialog)
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.window_handles), len(self.start_tabs))
        self.assertNotIn(new_window, self.marionette.window_handles)
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)

        self.marionette.switch_to_window(new_window)
        self.assert_window_handles()
        self.assertEqual(self.marionette.get_url(), self.chrome_dialog)

        # Check that the opened dialog is not accessible via window handles
        with self.assertRaises(errors.NoSuchWindowException):
            self.marionette.current_window_handle
        with self.assertRaises(errors.NoSuchWindowException):
            self.marionette.close()

        # Close the dialog and carry on in our original tab.
        self.marionette.close_chrome_window()
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.window_handles), len(self.start_tabs))

        self.marionette.switch_to_window(self.start_tab)
        self.assert_window_handles()
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)

    def test_window_handles_after_closing_original_tab(self):
        with self.marionette.using_context("content"):
            new_tab = self.open_tab()
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.window_handles), len(self.start_tabs) + 1)
        self.assertIn(new_tab, self.marionette.window_handles)
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)

        self.marionette.close()
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.window_handles), len(self.start_tabs))
        self.assertIn(new_tab, self.marionette.window_handles)

        self.marionette.switch_to_window(new_tab)
        self.assert_window_handles()
        self.assertEqual(self.marionette.current_window_handle, new_tab)

    def test_window_handles_after_closing_last_window(self):
        self.close_all_windows()
        self.assertEqual(self.marionette.close_chrome_window(), [])
