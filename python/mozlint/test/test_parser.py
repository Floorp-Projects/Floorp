# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from unittest import TestCase

from mozunit import main

from mozlint.parser import Parser
from mozlint.errors import (
    LinterNotFound,
    LinterParseError,
)


here = os.path.abspath(os.path.dirname(__file__))


class TestParser(TestCase):

    def __init__(self, *args, **kwargs):
        TestCase.__init__(self, *args, **kwargs)

        self._lintdir = os.path.join(here, 'linters')
        self._parse = Parser()

    def parse(self, name):
        return self._parse(os.path.join(self._lintdir, name))

    def test_parse_valid_linter(self):
        linter = self.parse('string.lint')
        self.assertIsInstance(linter, dict)
        self.assertIn('name', linter)
        self.assertIn('description', linter)
        self.assertIn('type', linter)
        self.assertIn('payload', linter)

    def test_parse_invalid_type(self):
        with self.assertRaises(LinterParseError):
            self.parse('invalid_type.lint')

    def test_parse_invalid_extension(self):
        with self.assertRaises(LinterParseError):
            self.parse('invalid_extension.lnt')

    def test_parse_invalid_include_exclude(self):
        with self.assertRaises(LinterParseError):
            self.parse('invalid_include.lint')

        with self.assertRaises(LinterParseError):
            self.parse('invalid_exclude.lint')

    def test_parse_missing_attributes(self):
        with self.assertRaises(LinterParseError):
            self.parse('missing_attrs.lint')

    def test_parse_missing_definition(self):
        with self.assertRaises(LinterParseError):
            self.parse('missing_definition.lint')

    def test_parse_non_existent_linter(self):
        with self.assertRaises(LinterNotFound):
            self.parse('missing_file.lint')


if __name__ == '__main__':
    main()
