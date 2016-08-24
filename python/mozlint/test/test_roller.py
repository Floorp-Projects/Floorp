# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys
from unittest import TestCase

from mozunit import main

from mozlint import LintRoller, ResultContainer
from mozlint.errors import LintersNotConfigured, LintException


here = os.path.abspath(os.path.dirname(__file__))


class TestLintRoller(TestCase):

    def __init__(self, *args, **kwargs):
        TestCase.__init__(self, *args, **kwargs)

        self.filedir = os.path.join(here, 'files')
        self.files = [os.path.join(self.filedir, f) for f in os.listdir(self.filedir)]
        self.lintdir = os.path.join(here, 'linters')

        names = ('string.lint', 'regex.lint', 'external.lint')
        self.linters = [os.path.join(self.lintdir, n) for n in names]

    def setUp(self):
        TestCase.setUp(self)
        self.lint = LintRoller(root=here)

    def test_roll_no_linters_configured(self):
        with self.assertRaises(LintersNotConfigured):
            self.lint.roll(self.files)

    def test_roll_successful(self):
        self.lint.read(self.linters)

        result = self.lint.roll(self.files)
        self.assertEqual(len(result), 1)

        path = result.keys()[0]
        self.assertEqual(os.path.basename(path), 'foobar.js')

        errors = result[path]
        self.assertIsInstance(errors, list)
        self.assertEqual(len(errors), 6)

        container = errors[0]
        self.assertIsInstance(container, ResultContainer)
        self.assertEqual(container.rule, 'no-foobar')

    def test_roll_catch_exception(self):
        self.lint.read(os.path.join(self.lintdir, 'raises.lint'))

        # suppress printed traceback from test output
        old_stderr = sys.stderr
        sys.stderr = open(os.devnull, 'w')
        with self.assertRaises(LintException):
            self.lint.roll(self.files)
        sys.stderr = old_stderr

    def test_roll_with_excluded_path(self):
        self.lint.lintargs.update({'exclude': ['**/foobar.js']})

        self.lint.read(self.linters)
        result = self.lint.roll(self.files)

        self.assertEqual(len(result), 0)

    def test_roll_with_invalid_extension(self):
        self.lint.read(os.path.join(self.lintdir, 'external.lint'))
        result = self.lint.roll(os.path.join(self.filedir, 'foobar.py'))
        self.assertEqual(len(result), 0)


if __name__ == '__main__':
    main()
