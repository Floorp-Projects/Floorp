# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_test import MarionetteTestCase


class TestEmulatorScreen(MarionetteTestCase):

    def setUp(self):
        MarionetteTestCase.setUp(self)

        self.screen = self.marionette.emulator.screen
        self.screen.initialize()

    def test_emulator_orientation(self):
        self.assertEqual(self.screen.orientation, self.screen.SO_PORTRAIT_PRIMARY,
                         'Orientation has been correctly initialized.')

        self.sensor.orientation = self.sensor.SO_PORTRAIT_SECONDARY
        self.assertEqual(self.sensor.orientation, self.sensor.SO_PORTRAIT_SECONDARY,
                         'Orientation has been set to portrait-secondary')

        self.sensor.orientation = self.sensor.SO_LANDSCAPE_PRIMARY
        self.assertEqual(self.sensor.orientation, self.sensor.SO_LANDSCAPE_PRIMARY,
                         'Orientation has been set to landscape-primary')

        self.sensor.orientation = self.sensor.SO_LANDSCAPE_SECONDARY
        self.assertEqual(self.sensor.orientation, self.sensor.SO_LANDSCAPE_SECONDARY,
                         'Orientation has been set to landscape-secondary')

        self.sensor.orientation = self.sensor.SO_PORTRAIT_PRIMARY
        self.assertEqual(self.sensor.orientation, self.sensor.SO_PORTRAIT_PRIMARY,
                         'Orientation has been set to portrait-primary')
