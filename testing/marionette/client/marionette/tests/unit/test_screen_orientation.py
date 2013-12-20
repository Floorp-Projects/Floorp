# -*- fill-column: 100; comment-column: 100; -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from emulator_screen import EmulatorScreen
from marionette import MarionetteException
from marionette_test import MarionetteTestCase

default_orientation = "portrait-primary"
unknown_orientation = "Unknown screen orientation: %s"

class TestScreenOrientation(MarionetteTestCase):
    def tearDown(self):
        self.marionette.set_orientation(default_orientation)
        self.assertEqual(self.marionette.orientation, default_orientation, "invalid state")
        super(MarionetteTestCase, self).tearDown()

    def test_set_orientation_to_portrait_primary(self):
        self.marionette.set_orientation("portrait-primary")
        new_orientation = self.marionette.orientation
        self.assertEqual(new_orientation, "portrait-primary")

        if self.marionette.emulator:
            emulator_orientation = self.marionette.emulator.screen.orientation
            self.assertEqual(emulator_orientation, EmulatorScreen.SO_PORTRAIT_PRIMARY)

    def test_set_orientation_to_landscape_primary(self):
        self.marionette.set_orientation("landscape-primary")
        new_orientation = self.marionette.orientation
        self.assertEqual(new_orientation, "landscape-primary")

        if self.marionette.emulator:
            emulator_orientation = self.marionette.emulator.screen.orientation
            self.assertEqual(emulator_orientation, EmulatorScreen.SO_LANDSCAPE_PRIMARY)

    def test_set_orientation_to_portrait_secondary(self):
        self.marionette.set_orientation("portrait-secondary")
        new_orientation = self.marionette.orientation
        self.assertEqual(new_orientation, "portrait-secondary")

        if self.marionette.emulator:
            emulator_orientation = self.marionette.emulator.screen.orientation
            self.assertEqual(emulator_orientation, EmulatorScreen.SO_PORTRAIT_SECONDARY)

    def test_set_orientation_to_landscape_secondary(self):
        self.marionette.set_orientation("landscape-secondary")
        new_orientation = self.marionette.orientation
        self.assertEqual(new_orientation, "landscape-secondary")

        if self.marionette.emulator:
            emulator_orientation = self.marionette.emulator.screen.orientation
            self.assertEqual(emulator_orientation, EmulatorScreen.SO_LANDSCAPE_SECONDARY)

    def test_set_orientation_to_shorthand_portrait(self):
        # Set orientation to something other than portrait-primary first, since the default is
        # portrait-primary.
        self.marionette.set_orientation("landscape-primary")
        self.assertEqual(self.marionette.orientation, "landscape-primary", "invalid state")

        self.marionette.set_orientation("portrait")
        new_orientation = self.marionette.orientation
        self.assertEqual(new_orientation, "portrait-primary")

        if self.marionette.emulator:
            emulator_orientation = self.marionette.emulator.screen.orientation
            self.assertEqual(emulator_orientation, EmulatorScreen.SO_PORTRAIT_PRIMARY)

    def test_set_orientation_to_shorthand_landscape(self):
        self.marionette.set_orientation("landscape")
        new_orientation = self.marionette.orientation
        self.assertEqual(new_orientation, "landscape-primary")

        if self.marionette.emulator:
            emulator_orientation = self.marionette.emulator.screen.orientation
            self.assertEqual(emulator_orientation, EmulatorScreen.SO_LANDSCAPE_PRIMARY)

    def test_set_orientation_with_mixed_casing(self):
        self.marionette.set_orientation("lAnDsCaPe")
        new_orientation = self.marionette.orientation
        self.assertEqual(new_orientation, "landscape-primary")

    def test_set_invalid_orientation(self):
        with self.assertRaisesRegexp(MarionetteException, unknown_orientation % "cheese"):
            self.marionette.set_orientation("cheese")

    def test_set_null_orientation(self):
        with self.assertRaisesRegexp(MarionetteException, unknown_orientation % "null"):
            self.marionette.set_orientation(None)
