# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import itertools

from marionette_driver import errors
from marionette.marionette_test import MarionetteTestCase as TC


class TestProtocol1Errors(TC):
    def setUp(self):
        TC.setUp(self)
        self.op = self.marionette.protocol
        self.marionette.protocol = 1

    def tearDown(self):
        self.marionette.protocol = self.op
        TC.tearDown(self)

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


class TestProtocol2Errors(TC):
    def setUp(self):
        TC.setUp(self)
        self.op = self.marionette.protocol
        self.marionette.protocol = 2

    def tearDown(self):
        self.marionette.protocol = self.op
        TC.tearDown(self)

    def test_malformed_packet(self):
        req = ["error", "message", "stacktrace"]
        ps = []
        for p in [p for i in range(0, len(req) + 1) for p in itertools.permutations(req, i)]:
            ps.append(dict((x, None) for x in p))

        for p in filter(lambda p: len(p) < 3, ps):
            self.assertRaises(KeyError, self.marionette._handle_error, p)

    def test_known_error_code(self):
        with self.assertRaises(errors.NoSuchElementException):
            self.marionette._handle_error(
                {"error": errors.NoSuchElementException.code[0],
                 "message": None,
                 "stacktrace": None})

    def test_known_error_status(self):
        with self.assertRaises(errors.NoSuchElementException):
            self.marionette._handle_error(
                {"error": errors.NoSuchElementException.status,
                 "message": None,
                 "stacktrace": None})

    def test_unknown_error_code(self):
        with self.assertRaises(errors.MarionetteException):
            self.marionette._handle_error(
                {"error": 123456,
                 "message": None,
                 "stacktrace": None})

    def test_unknown_error_status(self):
        with self.assertRaises(errors.MarionetteException):
            self.marionette._handle_error(
                {"error": "barbera",
                 "message": None,
                 "stacktrace": None})
