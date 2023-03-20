# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import unittest

import mozunit
import six

from mozpack.errors import AccumulatedErrors, ErrorMessage, errors


class TestErrors(object):
    def setUp(self):
        errors.out = six.moves.cStringIO()
        errors.ignore_errors(False)

    def tearDown(self):
        errors.out = sys.stderr

    def get_output(self):
        return [l.strip() for l in errors.out.getvalue().splitlines()]


class TestErrorsImpl(TestErrors, unittest.TestCase):
    def test_plain_error(self):
        errors.warn("foo")
        self.assertRaises(ErrorMessage, errors.error, "foo")
        self.assertRaises(ErrorMessage, errors.fatal, "foo")
        self.assertEqual(self.get_output(), ["warning: foo"])

    def test_ignore_errors(self):
        errors.ignore_errors()
        errors.warn("foo")
        errors.error("bar")
        self.assertRaises(ErrorMessage, errors.fatal, "foo")
        self.assertEqual(self.get_output(), ["warning: foo", "warning: bar"])

    def test_no_error(self):
        with errors.accumulate():
            errors.warn("1")

    def test_simple_error(self):
        with self.assertRaises(AccumulatedErrors):
            with errors.accumulate():
                errors.error("1")
        self.assertEqual(self.get_output(), ["error: 1"])

    def test_error_loop(self):
        with self.assertRaises(AccumulatedErrors):
            with errors.accumulate():
                for i in range(3):
                    errors.error("%d" % i)
        self.assertEqual(self.get_output(), ["error: 0", "error: 1", "error: 2"])

    def test_multiple_errors(self):
        with self.assertRaises(AccumulatedErrors):
            with errors.accumulate():
                errors.error("foo")
                for i in range(3):
                    if i == 2:
                        errors.warn("%d" % i)
                    else:
                        errors.error("%d" % i)
                errors.error("bar")
        self.assertEqual(
            self.get_output(),
            ["error: foo", "error: 0", "error: 1", "warning: 2", "error: bar"],
        )

    def test_errors_context(self):
        with self.assertRaises(AccumulatedErrors):
            with errors.accumulate():
                self.assertEqual(errors.get_context(), None)
                with errors.context("foo", 1):
                    self.assertEqual(errors.get_context(), ("foo", 1))
                    errors.error("a")
                    with errors.context("bar", 2):
                        self.assertEqual(errors.get_context(), ("bar", 2))
                        errors.error("b")
                    self.assertEqual(errors.get_context(), ("foo", 1))
                    errors.error("c")
        self.assertEqual(
            self.get_output(),
            [
                "error: foo:1: a",
                "error: bar:2: b",
                "error: foo:1: c",
            ],
        )


if __name__ == "__main__":
    mozunit.main()
