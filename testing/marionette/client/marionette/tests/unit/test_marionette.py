# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver import errors
from marionette import marionette_test


class TestHandleError(marionette_test.MarionetteTestCase):
    def test_malformed_packet(self):
        for t in [{}, {"error": None}]:
            with self.assertRaisesRegexp(errors.MarionetteException, "Malformed packet"):
                self.marionette._handle_error(t)

    def test_known_error_code(self):
        with self.assertRaises(errors.NoSuchElementException):
            self.marionette._handle_error(
                {"error": {"status": errors.NoSuchElementException.code[0]}})

    def test_known_error_status(self):
        with self.assertRaises(errors.NoSuchElementException):
            self.marionette._handle_error(
                {"error": {"status": errors.NoSuchElementException.status}})

    def test_unknown_error_code(self):
        with self.assertRaises(errors.MarionetteException):
            self.marionette._handle_error({"error": {"status": 123456}})

    def test_unknown_error_status(self):
        with self.assertRaises(errors.MarionetteException):
            self.marionette._handle_error({"error": {"status": "barbera"}})
