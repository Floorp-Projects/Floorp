# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_driver import errors
from marionette_driver.wait import Wait
from marionette_harness import (
    MarionetteTestCase,
    parameterized,
    skip_if_desktop,
    skip_if_mobile,
)


default_orientation = "portrait-primary"
unknown_orientation = "Unknown screen orientation: {}"


class TestScreenOrientation(MarionetteTestCase):

    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.is_mobile = self.marionette.session_capabilities.get("rotatable", False)

    def tearDown(self):
        if self.is_mobile:
            self.marionette.set_orientation(default_orientation)
            self.wait_for_orientation(default_orientation)
        MarionetteTestCase.tearDown(self)

    def wait_for_orientation(self, orientation, timeout=None):
        Wait(self.marionette, timeout=timeout).until(
            lambda _: self.marionette.orientation == orientation)

    @skip_if_desktop("Not supported in Firefox")
    @parameterized("landscape-primary", "landscape-primary")
    @parameterized("landscape-secondary", "landscape-secondary")
    @parameterized("portrait-primary", "portrait-primary")
    # @parameterized("portrait-secondary", "portrait-secondary") # Bug 1533084
    def test_set_orientation(self, orientation):
        self.marionette.set_orientation(orientation)
        self.wait_for_orientation(orientation)

    @skip_if_desktop("Not supported in Firefox")
    def test_set_orientation_to_shorthand_portrait(self):
        # Set orientation to something other than portrait-primary first,
        # since the default is portrait-primary.
        self.marionette.set_orientation("landscape-primary")
        self.wait_for_orientation("landscape-primary")

        self.marionette.set_orientation("portrait")
        self.wait_for_orientation("portrait-primary")

    @skip_if_desktop("Not supported in Firefox")
    def test_set_orientation_to_shorthand_landscape(self):
        self.marionette.set_orientation("landscape")
        self.wait_for_orientation("landscape-primary")

    @skip_if_desktop("Not supported in Firefox")
    def test_set_orientation_with_mixed_casing(self):
        self.marionette.set_orientation("lAnDsCaPe")
        self.wait_for_orientation("landscape-primary")

    @skip_if_desktop("Not supported in Firefox")
    def test_set_invalid_orientation(self):
        with self.assertRaisesRegexp(errors.MarionetteException,
                                     unknown_orientation.format("cheese")):
            self.marionette.set_orientation("cheese")

    @skip_if_desktop("Not supported in Firefox")
    def test_set_null_orientation(self):
        with self.assertRaisesRegexp(errors.MarionetteException,
                                     unknown_orientation.format("null")):
            self.marionette.set_orientation(None)

    @skip_if_mobile("Specific test for Firefox")
    def test_unsupported_operation_on_desktop(self):
        with self.assertRaises(errors.UnsupportedOperationException):
            self.marionette.set_orientation("landscape-primary")
