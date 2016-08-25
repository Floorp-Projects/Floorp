# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from unittest import TestCase

from mozunit import main

from mozlint import LintRoller
from mozlint.result import ResultContainer


here = os.path.abspath(os.path.dirname(__file__))


class TestLinterTypes(TestCase):

    def __init__(self, *args, **kwargs):
        TestCase.__init__(self, *args, **kwargs)

        self.lintdir = os.path.join(here, 'linters')
        self.filedir = os.path.join(here, 'files')
        self.files = [os.path.join(self.filedir, f) for f in os.listdir(self.filedir)]

    def setUp(self):
        TestCase.setUp(self)
        self.lint = LintRoller(root=here)

    def path(self, name):
        return os.path.join(self.filedir, name)

    def test_string_linter(self):
        self.lint.read(os.path.join(self.lintdir, 'string.lint'))
        result = self.lint.roll(self.files)
        self.assertIsInstance(result, dict)

        self.assertIn(self.path('foobar.js'), result.keys())
        self.assertNotIn(self.path('no_foobar.js'), result.keys())

        result = result[self.path('foobar.js')][0]
        self.assertIsInstance(result, ResultContainer)
        self.assertEqual(result.linter, 'StringLinter')

    def test_regex_linter(self):
        self.lint.read(os.path.join(self.lintdir, 'regex.lint'))
        result = self.lint.roll(self.files)
        self.assertIsInstance(result, dict)
        self.assertIn(self.path('foobar.js'), result.keys())
        self.assertNotIn(self.path('no_foobar.js'), result.keys())

        result = result[self.path('foobar.js')][0]
        self.assertIsInstance(result, ResultContainer)
        self.assertEqual(result.linter, 'RegexLinter')

    def test_external_linter(self):
        self.lint.read(os.path.join(self.lintdir, 'external.lint'))
        result = self.lint.roll(self.files)
        self.assertIsInstance(result, dict)
        self.assertIn(self.path('foobar.js'), result.keys())
        self.assertNotIn(self.path('no_foobar.js'), result.keys())

        result = result[self.path('foobar.js')][0]
        self.assertIsInstance(result, ResultContainer)
        self.assertEqual(result.linter, 'ExternalLinter')

    def test_no_filter(self):
        self.lint.read(os.path.join(self.lintdir, 'explicit_path.lint'))
        result = self.lint.roll(self.files)
        self.assertEqual(len(result), 0)

        self.lint.lintargs['use_filters'] = False
        result = self.lint.roll(self.files)
        self.assertEqual(len(result), 2)


if __name__ == '__main__':
    main()
