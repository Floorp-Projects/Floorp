# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys

import six

from marionette_driver import errors

from marionette_harness import marionette_test


def fake_cause():
    try:
        raise ValueError("bar")
    except ValueError:
        return sys.exc_info()


message = "foo"
unicode_message = "\u201Cfoo"
cause = fake_cause()
stacktrace = "first\nsecond"


class TestErrors(marionette_test.MarionetteTestCase):
    def test_defaults(self):
        exc = errors.MarionetteException()
        self.assertEqual(str(exc), "None")
        self.assertIsNone(exc.cause)
        self.assertIsNone(exc.stacktrace)

    def test_construction(self):
        exc = errors.MarionetteException(
            message=message, cause=cause, stacktrace=stacktrace
        )
        self.assertEqual(exc.message, message)
        self.assertEqual(exc.cause, cause)
        self.assertEqual(exc.stacktrace, stacktrace)

    def test_str_message(self):
        exc = errors.MarionetteException(
            message=message, cause=cause, stacktrace=stacktrace
        )
        r = str(exc)
        self.assertIn(message, r)
        self.assertIn(", caused by {0!r}".format(cause[0]), r)
        self.assertIn("\nstacktrace:\n\tfirst\n\tsecond", r)

    def test_unicode_message(self):
        exc = errors.MarionetteException(
            message=unicode_message, cause=cause, stacktrace=stacktrace
        )
        r = six.text_type(exc)
        self.assertIn(unicode_message, r)
        self.assertIn(", caused by {0!r}".format(cause[0]), r)
        self.assertIn("\nstacktrace:\n\tfirst\n\tsecond", r)

    def test_unicode_message_as_str(self):
        exc = errors.MarionetteException(
            message=unicode_message, cause=cause, stacktrace=stacktrace
        )
        r = str(exc)
        self.assertIn(six.ensure_str(unicode_message, encoding="utf-8"), r)
        self.assertIn(", caused by {0!r}".format(cause[0]), r)
        self.assertIn("\nstacktrace:\n\tfirst\n\tsecond", r)

    def test_cause_string(self):
        exc = errors.MarionetteException(cause="foo")
        self.assertEqual(exc.cause, "foo")
        r = str(exc)
        self.assertIn(", caused by foo", r)

    def test_cause_tuple(self):
        exc = errors.MarionetteException(cause=cause)
        self.assertEqual(exc.cause, cause)
        r = str(exc)
        self.assertIn(", caused by {0!r}".format(cause[0]), r)


class TestLookup(marionette_test.MarionetteTestCase):
    def test_by_unknown_number(self):
        self.assertEqual(errors.MarionetteException, errors.lookup(123456))

    def test_by_known_string(self):
        self.assertEqual(
            errors.NoSuchElementException, errors.lookup("no such element")
        )

    def test_by_unknown_string(self):
        self.assertEqual(errors.MarionetteException, errors.lookup("barbera"))

    def test_by_known_unicode_string(self):
        self.assertEqual(
            errors.NoSuchElementException, errors.lookup("no such element")
        )


class TestAllErrors(marionette_test.MarionetteTestCase):
    def test_properties(self):
        for exc in errors.es_:
            self.assertTrue(
                hasattr(exc, "status"), "expected exception to have attribute `status'"
            )
