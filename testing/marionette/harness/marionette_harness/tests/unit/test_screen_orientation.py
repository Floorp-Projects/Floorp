# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver import errors
from mozrunner.devices.emulator_screen import EmulatorScreen

from marionette_harness import MarionetteTestCase, skip_if_desktop, skip_if_mobile


default_orientation = "portrait-primary"
unknown_orientation = "Unknown screen orientation: {}"


class TestScreenOrientation(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.is_mobile = self.marionette.session_capabilities.get("rotatable", False)

    def tearDown(self):
        if self.is_mobile:
            self.marionette.set_orientation(default_orientation)
            self.assertEqual(self.marionette.orientation, default_orientation, "invalid state")
        MarionetteTestCase.tearDown(self)

    @skip_if_desktop("Not supported in Firefox")
    def test_set_orientation_to_portrait_primary(self):
        self.marionette.set_orientation("portrait-primary")
        new_orientation = self.marionette.orientation
        self.assertEqual(new_orientation, "portrait-primary")

    @skip_if_desktop("Not supported in Firefox")
    def test_set_orientation_to_landscape_primary(self):
        self.marionette.set_orientation("landscape-primary")
        new_orientation = self.marionette.orientation
        self.assertEqual(new_orientation, "landscape-primary")

    @skip_if_desktop("Not supported in Firefox")
    def test_set_orientation_to_portrait_secondary(self):
        self.marionette.set_orientation("portrait-secondary")
        new_orientation = self.marionette.orientation
        self.assertEqual(new_orientation, "portrait-secondary")

    @skip_if_desktop("Not supported in Firefox")
    def test_set_orientation_to_landscape_secondary(self):
        self.marionette.set_orientation("landscape-secondary")
        new_orientation = self.marionette.orientation
        self.assertEqual(new_orientation, "landscape-secondary")

    @skip_if_desktop("Not supported in Firefox")
    def test_set_orientation_to_shorthand_portrait(self):
        # Set orientation to something other than portrait-primary first, since the default is
        # portrait-primary.
        self.marionette.set_orientation("landscape-primary")
        self.assertEqual(self.marionette.orientation, "landscape-primary", "invalid state")

        self.marionette.set_orientation("portrait")
        new_orientation = self.marionette.orientation
        self.assertEqual(new_orientation, "portrait-primary")

    @skip_if_desktop("Not supported in Firefox")
    def test_set_orientation_to_shorthand_landscape(self):
        self.marionette.set_orientation("landscape")
        new_orientation = self.marionette.orientation
        self.assertEqual(new_orientation, "landscape-primary")

    @skip_if_desktop("Not supported in Firefox")
    def test_set_orientation_with_mixed_casing(self):
        self.marionette.set_orientation("lAnDsCaPe")
        new_orientation = self.marionette.orientation
        self.assertEqual(new_orientation, "landscape-primary")

    @skip_if_desktop("Not supported in Firefox")
    def test_set_invalid_orientation(self):
        with self.assertRaisesRegexp(errors.MarionetteException, unknown_orientation.format("cheese")):
            self.marionette.set_orientation("cheese")

    @skip_if_desktop("Not supported in Firefox")
    def test_set_null_orientation(self):
        with self.assertRaisesRegexp(errors.MarionetteException, unknown_orientation.format("null")):
            self.marionette.set_orientation(None)

    @skip_if_mobile("Specific test for Firefox")
    def test_unsupported_operation_on_desktop(self):
        with self.assertRaises(errors.UnsupportedOperationException):
            self.marionette.set_orientation("landscape-primary")
