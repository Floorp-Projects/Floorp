#Copyright 2007-2009 WebDriver committers
#
#Licensed under the Apache License, Version 2.0 (the "License");
#you may not use this file except in compliance with the License.
#You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
#Unless required by applicable law or agreed to in writing, software
#distributed under the License is distributed on an "AS IS" BASIS,
#WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#See the License for the specific language governing permissions and
#limitations under the License.

from marionette_driver.errors import InvalidArgumentException

from marionette_harness import MarionetteTestCase


class TestWindowPosition(MarionetteTestCase):
    def test_get_types(self):
        position = self.marionette.get_window_position()
        self.assertTrue(isinstance(position["x"], int))
        self.assertTrue(isinstance(position["y"], int))

    def test_set_types(self):
        for x, y in (["a", "b"], [1.2, 3.4], [True, False], [[], []], [{}, {}]):
            with self.assertRaises(InvalidArgumentException):
                self.marionette.set_window_position(x, y)

    def test_out_of_bounds_arguments(self):
        with self.assertRaises(InvalidArgumentException):
            self.marionette.set_window_position(-1, 0)
        with self.assertRaises(InvalidArgumentException):
            self.marionette.set_window_position(0, -1)

    def test_move(self):
        old_position = self.marionette.get_window_position()
        new_position = {"x": old_position["x"] + 10, "y": old_position["y"] + 10}
        self.marionette.set_window_position(new_position["x"], new_position["y"])
        self.assertNotEqual(old_position['x'], new_position["x"])
        self.assertNotEqual(old_position['y'], new_position["y"])
