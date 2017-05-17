# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver.errors import InvalidArgumentException

from marionette_harness import MarionetteTestCase


class TestPosition(MarionetteTestCase):

    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.original_position = self.marionette.get_window_position()

    def tearDown(self):
        x, y = self.original_position["x"], self.original_position["y"]
        self.marionette.set_window_position(x, y)
        MarionetteTestCase.tearDown(self)

    def test_get_types(self):
        position = self.marionette.get_window_position()
        self.assertIn("x", position)
        self.assertIn("y", position)
        self.assertIsInstance(position["x"], int)
        self.assertIsInstance(position["y"], int)

    def test_set_types(self):
        for x, y in (["a", "b"], [1.2, 3.4], [True, False], [[], []], [{}, {}]):
            print("testing invalid type position ({},{})".format(x, y))
            with self.assertRaises(InvalidArgumentException):
                self.marionette.set_window_position(x, y)

    def test_setting_window_rect_with_nulls_errors(self):
        with self.assertRaises(InvalidArgumentException):
            self.marionette.set_window_rect(height=None, width=None,
                                            x=None, y=None)

    def test_set_position_with_rect(self):
        old_position = self.marionette.window_rect
        wanted_position = {"x": old_position["x"] + 10, "y": old_position["y"] + 10}

        new_position = self.marionette.set_window_rect(x=wanted_position["x"], y=wanted_position["y"])

        self.assertNotEqual(old_position["x"], new_position["x"])
        self.assertNotEqual(old_position["y"], new_position["y"])

    def test_move_to_new_position(self):
        old_position = self.marionette.get_window_position()
        new_position = {"x": old_position["x"] + 10, "y": old_position["y"] + 10}
        self.marionette.set_window_position(new_position["x"], new_position["y"])
        self.assertNotEqual(old_position["x"], new_position["x"])
        self.assertNotEqual(old_position["y"], new_position["y"])

    def test_move_to_existing_position(self):
        old_position = self.marionette.get_window_position()
        self.marionette.set_window_position(old_position["x"], old_position["y"])
        new_position = self.marionette.get_window_position()
        self.assertEqual(old_position["x"], new_position["x"])
        self.assertEqual(old_position["y"], new_position["y"])

    def test_move_to_negative_coordinates(self):
        print("Current position: {}".format(
            self.marionette.get_window_position()))
        self.marionette.set_window_position(-8, -8)
        position = self.marionette.get_window_position()
        print("Position after requesting move to negative coordinates: {}".format(position))

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

        # Certain WMs prohibit windows from being moved off-screen,
        # but we don't have this information.  It should be safe to
        # assume a window can be moved to (0,0) or less.
        if os == "linux":
            # certain WMs prohibit windows from being moved off-screen
            self.assertLessEqual(position["x"], 0)
            self.assertLessEqual(position["y"], 0)

        # On macOS, windows can only be moved off the screen on the
        # horizontal axis.  The system menu bar also blocks windows from
        # being moved to (0,0).
        elif os == "darwin":
            self.assertEqual(-8, position["x"])
            self.assertEqual(23, position["y"])

        # It turns out that Windows is the only platform on which the
        # window can be reliably positioned off-screen.
        elif os == "windows_nt":
            self.assertEqual(-8, position["x"])
            self.assertEqual(-8, position["y"])


class TestSize(MarionetteTestCase):

    def setUp(self):
        super(MarionetteTestCase, self).setUp()
        self.max = self.marionette.execute_script("""
            return {
              width: window.screen.availWidth,
              height: window.screen.availHeight,
            }""", sandbox=None)

        # WebDriver spec says a resize cannot result in window being
        # maximised, an error is returned if that is the case; therefore if
        # the window is maximised at the start of this test, returning to
        # the original size via set_window_size size will result in error;
        # so reset to original size minus 1 pixel width
        start_size = self.marionette.window_size
        if start_size["width"] == self.max["width"] and start_size["height"] == self.max["height"]:
            start_size["width"] -= 1
            start_size["height"] -= 1
        self.marionette.set_window_size(start_size["width"], start_size["height"])

        self.original_size = self.marionette.window_size

    def tearDown(self):
        self.marionette.set_window_size(
            self.original_size["width"], self.original_size["height"])
        super(MarionetteTestCase, self).tearDown()

    def test_get_types(self):
        size = self.marionette.window_size
        self.assertIn("width", size)
        self.assertIn("height", size)
        self.assertIsInstance(size["width"], int)
        self.assertIsInstance(size["height"], int)

    def test_set_types(self):
        for width, height in (["a", "b"], [1.2, 3.4], [True, False], [[], []], [{}, {}]):
            print("testing invalid type size ({},{})".format(width, height))
            with self.assertRaises(InvalidArgumentException):
                self.marionette.set_window_size(width, height)

    def test_setting_window_rect_with_nulls_errors(self):
        with self.assertRaises(InvalidArgumentException):
            self.marionette.set_window_rect(height=None, width=None,
                                            x=None, y=None)

    def test_set_size_with_rect(self):
        actual = self.marionette.window_size
        width = actual["width"] - 50
        height = actual["height"] - 50

        size = self.marionette.set_window_rect(width=width, height=height)
        self.assertEqual(size["width"], width,
                         "New width is {0} but should be {1}".format(size["width"], width))
        self.assertEqual(size["height"], height,
                         "New height is {0} but should be {1}".format(size["height"], height))

    def test_resize_to_new_size(self):
        old = self.marionette.window_size
        new = {"width": old["width"] + 10, "height": old["height"] + 10}
        self.marionette.set_window_size(new["width"], new["height"])
        actual = self.marionette.window_size
        self.assertEqual(actual["width"], new["width"])
        self.assertEqual(actual["height"], new["height"])

    def test_resize_to_existing_size(self):
        old = self.marionette.window_size
        self.marionette.set_window_size(old["width"], old["height"])
        new = self.marionette.window_size
        self.assertEqual(old["width"], new["width"])
        self.assertEqual(old["height"], new["height"])

    def test_resize_larger_than_screen(self):
        self.marionette.set_window_size(
            self.max["width"] * 2, self.max["height"] * 2)
        new = self.marionette.window_size

        # in X the window size may be greater than the bounds of the screen
        self.assertGreaterEqual(new["width"], self.max["width"])
        self.assertGreaterEqual(new["height"], self.max["height"])

    def test_resize_to_available_screen_size(self):
        result = self.marionette.set_window_rect(width=self.max['width'],
                                                 height=self.max["height"])
        self.assertEqual(result["width"], self.max["width"])
        self.assertEqual(result["height"], self.max["height"])
