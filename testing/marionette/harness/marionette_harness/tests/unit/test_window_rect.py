# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

from marionette_driver.errors import InvalidArgumentException
from marionette_harness import MarionetteTestCase


class TestWindowRect(MarionetteTestCase):

    def setUp(self):
        super(TestWindowRect, self).setUp()

        self.original_rect = self.marionette.window_rect

        self.max = self.marionette.execute_script("""
            return {
              width: window.screen.availWidth,
              height: window.screen.availHeight,
            }""", sandbox=None)

        # WebDriver spec says a resize cannot result in window being
        # maximised, an error is returned if that is the case; therefore if
        # the window is maximised at the start of this test, returning to
        # the original size via set_window_rect size will result in error;
        # so reset to original size minus 1 pixel width
        start_size = {"height": self.original_rect["height"], "width": self.original_rect["width"]}
        if start_size["width"] == self.max["width"] and start_size["height"] == self.max["height"]:
            start_size["width"] -= 10
            start_size["height"] -= 10
        self.marionette.set_window_rect(height=start_size["height"], width=start_size["width"])

    def tearDown(self):
        x, y = self.original_rect["x"], self.original_rect["y"]
        height, width = self.original_rect["height"], self.original_rect["width"]

        self.marionette.set_window_rect(x=x, y=y, height=height, width=width)

        is_fullscreen = self.marionette.execute_script("return document.fullscreenElement;", sandbox=None)
        if is_fullscreen:
            self.marionette.fullscreen()

        super(TestWindowRect, self).tearDown()

    def test_get_types(self):
        rect = self.marionette.window_rect
        self.assertIn("x", rect)
        self.assertIn("y", rect)
        self.assertIn("height", rect)
        self.assertIn("width", rect)
        self.assertIsInstance(rect["x"], int)
        self.assertIsInstance(rect["y"], int)
        self.assertIsInstance(rect["height"], int)
        self.assertIsInstance(rect["width"], int)

    def test_set_types(self):
        invalid_rects = (["a", "b", "h", "w"],
                         [1.2, 3.4, 4.5, 5.6],
                         [True, False, True, False],
                         [[], [], [], []],
                         [{}, {}, {}, {}])
        for x, y, h, w in invalid_rects:
            print("testing invalid type position ({},{})".format(x, y))
            with self.assertRaises(InvalidArgumentException):
                self.marionette.set_window_rect(x=x, y=y, height=h, width=w)

    def test_setting_window_rect_with_nulls_errors(self):
        with self.assertRaises(InvalidArgumentException):
            self.marionette.set_window_rect(height=None, width=None,
                                            x=None, y=None)

    def test_set_position(self):
        old_position = self.marionette.window_rect
        wanted_position = {"x": old_position["x"] + 10, "y": old_position["y"] + 10}

        new_position = self.marionette.set_window_rect(x=wanted_position["x"], y=wanted_position["y"])
        expected_position = self.marionette.window_rect

        self.assertEqual(new_position["x"], wanted_position["x"])
        self.assertEqual(new_position["y"], wanted_position["y"])
        self.assertEqual(new_position["x"], expected_position["x"])
        self.assertEqual(new_position["y"], expected_position["y"])

    def test_set_size(self):
        old_size = self.marionette.window_rect
        wanted_size = {"height": old_size["height"] - 50, "width": old_size["width"] - 50}

        new_size = self.marionette.set_window_rect(height=wanted_size["height"], width=wanted_size["width"])
        expected_size = self.marionette.window_rect

        self.assertEqual(new_size["width"], wanted_size["width"],
                         "New width is {0} but should be {1}".format(new_size["width"], wanted_size["width"]))
        self.assertEqual(new_size["height"], wanted_size["height"],
                         "New height is {0} but should be {1}".format(new_size["height"], wanted_size["height"]))
        self.assertEqual(new_size["width"], expected_size["width"],
                         "New width is {0} but should be {1}".format(new_size["width"], expected_size["width"]))
        self.assertEqual(new_size["height"], expected_size["height"],
                         "New height is {0} but should be {1}".format(new_size["height"], expected_size["height"]))

    def test_set_position_and_size(self):
        old_rect = self.marionette.window_rect
        wanted_rect = {"x": old_rect["x"] + 10, "y": old_rect["y"] + 10,
                       "width": old_rect["width"] - 50, "height": old_rect["height"] - 50}

        new_rect = self.marionette.set_window_rect(x=wanted_rect["x"], y=wanted_rect["y"],
                                                   width=wanted_rect["width"], height=wanted_rect["height"])
        expected_rect = self.marionette.window_rect

        self.assertEqual(new_rect["x"], wanted_rect["x"])
        self.assertEqual(new_rect["y"], wanted_rect["y"])
        self.assertEqual(new_rect["width"], wanted_rect["width"],
                         "New width is {0} but should be {1}".format(new_rect["width"], wanted_rect["width"]))
        self.assertEqual(new_rect["height"], wanted_rect["height"],
                         "New height is {0} but should be {1}".format(new_rect["height"], wanted_rect["height"]))
        self.assertEqual(new_rect["x"], expected_rect["x"])
        self.assertEqual(new_rect["y"], expected_rect["y"])
        self.assertEqual(new_rect["width"], expected_rect["width"],
                         "New width is {0} but should be {1}".format(new_rect["width"], expected_rect["width"]))
        self.assertEqual(new_rect["height"], expected_rect["height"],
                         "New height is {0} but should be {1}".format(new_rect["height"], expected_rect["height"]))

    def test_move_to_current_position(self):
        old_position = self.marionette.window_rect
        new_position = self.marionette.set_window_rect(x=old_position["x"], y=old_position["y"])

        self.assertEqual(new_position["x"], old_position["x"])
        self.assertEqual(new_position["y"], old_position["y"])

    def test_move_to_current_size(self):
        old_size = self.marionette.window_rect
        new_size = self.marionette.set_window_rect(height=old_size["height"], width=old_size["width"])

        self.assertEqual(new_size["height"], old_size["height"])
        self.assertEqual(new_size["width"], old_size["width"])

    def test_move_to_current_position_and_size(self):
        old_position_and_size = self.marionette.window_rect
        new_position_and_size = self.marionette.set_window_rect(x=old_position_and_size["x"], y=old_position_and_size["y"],
                                                                height=old_position_and_size["height"], width=old_position_and_size["width"])

        self.assertEqual(new_position_and_size["x"], old_position_and_size["x"])
        self.assertEqual(new_position_and_size["y"], old_position_and_size["y"])
        self.assertEqual(new_position_and_size["width"], old_position_and_size["width"])
        self.assertEqual(new_position_and_size["height"], old_position_and_size["height"])

    def test_move_to_negative_coordinates(self):
        old_position = self.marionette.window_rect
        print("Current position: {}".format(
            old_position["x"], old_position["y"]))
        new_position = self.marionette.set_window_rect(x=-8, y=-8)
        print("Position after requesting move to negative coordinates: {}, {}"
              .format(new_position["x"], new_position["y"]))

        # Different systems will report more or less than (-8,-8)
        # depending on the characteristics of the window manager, since
        # the screenX/screenY position measures the chrome boundaries,
        # including any WM decorations.
        #
        # This makes this hard to reliably test across different
        # environments.  Generally we are happy when calling
        # marionette.set_window_position with negative coordinates does
        # not throw.
        #
        # Because we have to cater to an unknown set of environments,
        # the following assertions are the most common denominator that
        # make this test pass, irregardless of system characteristics.

        os = self.marionette.session_capabilities["platformName"]

        # Regardless of platform, headless always supports being positioned
        # off-screen.
        if self.marionette.session_capabilities["moz:headless"]:
            self.assertEqual(-8, new_position["x"])
            self.assertEqual(-8, new_position["y"])

        # Certain WMs prohibit windows from being moved off-screen,
        # but we don't have this information.  It should be safe to
        # assume a window can be moved to (0,0) or less.
        elif os == "linux":
            # certain WMs prohibit windows from being moved off-screen
            self.assertLessEqual(new_position["x"], 0)
            self.assertLessEqual(new_position["y"], 0)

        # On macOS, windows can only be moved off the screen on the
        # horizontal axis.  The system menu bar also blocks windows from
        # being moved to (0,0).
        elif os == "mac":
            self.assertEqual(-8, new_position["x"])
            self.assertEqual(23, new_position["y"])

        # It turns out that Windows is the only platform on which the
        # window can be reliably positioned off-screen.
        elif os == "windows":
            self.assertEqual(-8, new_position["x"])
            self.assertEqual(-8, new_position["y"])

    def test_resize_larger_than_screen(self):
        new_size = self.marionette.set_window_rect(
            width=self.max["width"] * 2, height=self.max["height"] * 2)
        actual_size = self.marionette.window_rect

        # in X the window size may be greater than the bounds of the screen
        self.assertGreaterEqual(new_size["width"], self.max["width"])
        self.assertGreaterEqual(new_size["height"], self.max["height"])
        self.assertEqual(actual_size["width"], new_size["width"])
        self.assertEqual(actual_size["height"], new_size["height"])

    def test_resize_to_available_screen_size(self):
        expected_size = self.marionette.set_window_rect(width=self.max["width"],
                                                        height=self.max["height"])
        result_size = self.marionette.window_rect

        self.assertGreaterEqual(expected_size["width"], self.max["width"])
        self.assertGreaterEqual(expected_size["height"], self.max["height"])
        self.assertEqual(result_size["width"], expected_size["width"])
        self.assertEqual(result_size["height"], expected_size["height"])

    def test_resize_while_fullscreen(self):
        self.marionette.fullscreen()
        expected_size = self.marionette.set_window_rect(width=self.max["width"] - 100,
                                                        height=self.max["height"] - 100)
        result_size = self.marionette.window_rect

        self.assertTrue(self.marionette.execute_script("return window.fullscreenElement == null",
                                                        sandbox=None))
        self.assertEqual(self.max["width"] - 100, expected_size["width"])
        self.assertEqual(self.max["height"] - 100, expected_size["height"])
        self.assertEqual(result_size["width"], expected_size["width"])
        self.assertEqual(result_size["height"], expected_size["height"])
