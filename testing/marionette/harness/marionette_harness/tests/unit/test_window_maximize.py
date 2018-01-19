# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_harness import MarionetteTestCase


class TestWindowMaximize(MarionetteTestCase):

    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.max = self.marionette.execute_script("""
            return {
              width: window.screen.availWidth,
              height: window.screen.availHeight,
            }""", sandbox=None)

        # ensure window is not maximized
        self.marionette.set_window_size(
            self.max["width"] - 100, self.max["height"] - 100)
        actual = self.marionette.window_size
        self.assertNotEqual(actual["width"], self.max["width"])
        self.assertNotEqual(actual["height"], self.max["height"])

        self.original_size = actual

    def tearDown(self):
        self.marionette.set_window_size(
            self.original_size["width"], self.original_size["height"])

    def assert_window_maximized(self, actual, delta=None):
        if self.marionette.session_capabilities["platformName"] == "windows_nt":
            delta = 16
        else:
            delta = 8

        self.assertGreaterEqual(
            actual["width"], self.max["width"] - delta,
            msg="Window width is not within {delta} px of availWidth: "
                "current width {current} should be greater than or equal to max width {max}"
                .format(delta=delta, current=actual["width"], max=self.max["width"] - delta))
        self.assertGreaterEqual(
            actual["height"], self.max["height"] - delta,
            msg="Window height is not within {delta} px of availHeight: "
                "current height {current} should be greater than or equal to max height {max}"
                .format(delta=delta, current=actual["height"], max=self.max["height"] - delta))

    def assert_window_rect(self, rect):
        self.assertIn("width", rect)
        self.assertIn("height", rect)
        self.assertIn("x", rect)
        self.assertIn("y", rect)
        self.assertIsInstance(rect["width"], int)
        self.assertIsInstance(rect["height"], int)
        self.assertIsInstance(rect["x"], int)
        self.assertIsInstance(rect["y"], int)

    def test_maximize(self):
        maximize_resp = self.marionette.maximize_window()
        self.assert_window_rect(maximize_resp)
        window_rect_resp = self.marionette.window_rect
        self.assertEqual(maximize_resp, window_rect_resp)
        self.assert_window_maximized(maximize_resp)

    def test_maximize_twice_is_idempotent(self):
        maximized = self.marionette.maximize_window()
        self.assert_window_maximized(maximized)

        still_maximized = self.marionette.maximize_window()
        self.assert_window_maximized(still_maximized)

    def test_stress(self):
        for i in range(1, 25):
            expect_maximized = bool(i % 2)

            if expect_maximized:
                rect = self.marionette.maximize_window()
                self.assert_window_maximized(rect)
            else:
                rect = self.marionette.set_window_rect(width=800, height=600)
                self.assertEqual(800, rect["width"])
                self.assertEqual(600, rect["height"])
