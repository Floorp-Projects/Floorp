# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import types
import urllib

from marionette_driver import errors

from marionette_harness import MarionetteTestCase, skip_if_mobile, WindowManagerMixin


def inline(doc):
    return "data:text/html;charset=utf-8,{}".format(urllib.quote(doc))


class TestWindowHandles(WindowManagerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestWindowHandles, self).setUp()

        self.xul_dialog = "chrome://marionette/content/test_dialog.xul"

    def tearDown(self):
        self.close_all_tabs()

        super(TestWindowHandles, self).tearDown()

    def assert_window_handles(self):
        try:
            self.assertIsInstance(self.marionette.current_window_handle, types.StringTypes)
        except errors.NoSuchWindowException:
            pass

        for handle in self.marionette.window_handles:
            self.assertIsInstance(handle, types.StringTypes)

    def tst_window_handles_after_opening_new_tab(self):
        new_tab = self.open_tab()
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.window_handles), len(self.start_tabs) + 1)
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)

        self.marionette.switch_to_window(new_tab)
        self.assert_window_handles()
        self.assertEqual(self.marionette.current_window_handle, new_tab)

        self.marionette.switch_to_window(self.start_tab)
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)

        self.marionette.switch_to_window(new_tab)
        self.marionette.close()
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.window_handles), len(self.start_tabs))

        self.marionette.switch_to_window(self.start_tab)
        self.assert_window_handles()
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)

    def tst_window_handles_after_opening_new_browser_window(self):
        new_tab = self.open_window()
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.window_handles), len(self.start_tabs) + 1)
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)

        self.marionette.switch_to_window(new_tab)
        self.assert_window_handles()
        self.assertEqual(self.marionette.current_window_handle, new_tab)

        # Close the opened window and carry on in our original tab.
        self.marionette.close()
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.window_handles), len(self.start_tabs))

        self.marionette.switch_to_window(self.start_tab)
        self.assert_window_handles()
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)

    @skip_if_mobile("Fennec doesn't support other chrome windows")
    def tst_window_handles_after_opening_new_non_browser_window(self):
        new_window = self.open_chrome_window(self.xul_dialog)
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.window_handles), len(self.start_tabs))
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)
        self.assertNotIn(new_window, self.marionette.window_handles)

        self.marionette.switch_to_window(new_window)
        self.assert_window_handles()

        # Check that the opened window is not accessible via window handles
        with self.assertRaises(errors.NoSuchWindowException):
            self.marionette.current_window_handle
        with self.assertRaises(errors.NoSuchWindowException):
            self.marionette.close()

        # Close the opened window and carry on in our original tab.
        self.marionette.close_chrome_window()
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.window_handles), len(self.start_tabs))

        self.marionette.switch_to_window(self.start_tab)
        self.assert_window_handles()
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)

    def test_window_handles_after_closing_original_tab(self):
        new_tab = self.open_tab()
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.window_handles), len(self.start_tabs) + 1)
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)
        self.assertIn(new_tab, self.marionette.window_handles)

        self.marionette.close()
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.window_handles), len(self.start_tabs))
        self.assertNotIn(self.start_tab, self.marionette.window_handles)

        self.marionette.switch_to_window(new_tab)
        self.assert_window_handles()
        self.assertEqual(self.marionette.current_window_handle, new_tab)

    def tst_window_handles_after_closing_last_tab(self):
        self.close_all_tabs()
        self.assertEqual(self.marionette.close(), [])
