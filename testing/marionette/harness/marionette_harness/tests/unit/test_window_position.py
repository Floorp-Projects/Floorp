# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver.errors import InvalidArgumentException

from marionette_harness import MarionetteTestCase


class TestWindowPosition(MarionetteTestCase):
    def test_get_types(self):
        position = self.marionette.get_window_position()
        self.assertIsInstance(position["x"], int)
        self.assertIsInstance(position["y"], int)

    def test_set_types(self):
        for x, y in (["a", "b"], [1.2, 3.4], [True, False], [[], []], [{}, {}]):
            print("testing invalid type position ({},{})".format(x, y))
            with self.assertRaises(InvalidArgumentException):
                self.marionette.set_window_position(x, y)

    def test_out_of_bounds_arguments(self):
        for x, y in [(-1,0), (0,-1), (-1,-1)]:
            print("testing out of bounds position ({},{})".format(x, y))
            with self.assertRaises(InvalidArgumentException):
                self.marionette.set_window_position(x, y)

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
