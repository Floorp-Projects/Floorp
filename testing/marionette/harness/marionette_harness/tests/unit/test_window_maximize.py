# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver.errors import InvalidArgumentException

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

        self.assertAlmostEqual(
            actual["width"], self.max["width"],
            delta=delta,
            msg="Window width is not within {} px of availWidth: "
                "current width {} and max width {}"
                .format(delta, actual["width"], self.max["width"]))
        self.assertAlmostEqual(
            actual["height"], self.max["height"],
            delta=delta,
            msg="Window height is not within {} px of availHeight, "
                "current height {} and max height {}"
                .format(delta, actual["height"], self.max["height"]))

    def assert_window_restored(self, actual):
        self.assertEqual(self.original_size["width"], actual["width"])
        self.assertEqual(self.original_size["height"], actual["height"])

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
        rect = self.marionette.maximize_window()
        self.assert_window_rect(rect)
        size = self.marionette.window_size
        self.assertEqual(size, rect)
        self.assert_window_maximized(size)

    def test_maximize_twice_restores(self):
        maximized = self.marionette.maximize_window()
        self.assert_window_maximized(maximized)

        restored = self.marionette.maximize_window()
        self.assert_window_restored(restored)

    def test_stress(self):
        for i in range(1, 25):
            expect_maximized = bool(i % 2)

            rect = self.marionette.maximize_window()
            if expect_maximized:
                self.assert_window_maximized(rect)
            else:
                self.assert_window_restored(rect)
