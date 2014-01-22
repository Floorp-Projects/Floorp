# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys

import errors
import marionette_test

from errors import ErrorCodes

def fake_cause():
    try:
        raise ValueError("bar")
    except ValueError as e:
        return sys.exc_info()

message = "foo"
status = ErrorCodes.TIMEOUT
cause = fake_cause()
stacktrace = "first\nsecond"

class TestMarionetteException(marionette_test.MarionetteTestCase):
    def test_defaults(self):
        exc = errors.MarionetteException()
        self.assertIsNone(exc.msg)
        self.assertEquals(exc.status, ErrorCodes.MARIONETTE_ERROR)
        self.assertIsNone(exc.cause)
        self.assertIsNone(exc.stacktrace)

    def test_construction(self):
        exc = errors.MarionetteException(
            message=message, status=status, cause=cause, stacktrace=stacktrace)
        self.assertEquals(exc.msg, message)
        self.assertEquals(exc.status, status)
        self.assertEquals(exc.cause, cause)
        self.assertEquals(exc.stacktrace, stacktrace)

    def test_str(self):
        exc = errors.MarionetteException(
            message=message, status=status, cause=cause, stacktrace=stacktrace)
        s = str(exc)
        self.assertIn(message, s)
        self.assertIn(", caused by %r" % cause[0], s)
        self.assertIn("\nstacktrace:\n\tfirst\n\tsecond\n", s)
        self.assertIn("MarionetteException:", s)
