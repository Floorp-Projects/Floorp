# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest

from mozunit import main

from mozbuild.util import (
    List,
    ReadOnlyDefaultDict,
    ReadOnlyDict,
)

class TestReadOnlyDict(unittest.TestCase):
    def test_basic(self):
        original = {'foo': 1, 'bar': 2}

        test = ReadOnlyDict(original)

        self.assertEqual(original, test)
        self.assertEqual(test['foo'], 1)

        with self.assertRaises(KeyError):
            value = test['missing']

        with self.assertRaises(Exception):
            test['baz'] = True


class TestReadOnlyDefaultDict(unittest.TestCase):
    def test_simple(self):
        original = {'foo': 1, 'bar': 2}

        test = ReadOnlyDefaultDict(bool, original)

        self.assertEqual(original, test)

        self.assertEqual(test['foo'], 1)

    def test_assignment(self):
        test = ReadOnlyDefaultDict(bool, {})

        with self.assertRaises(Exception):
            test['foo'] = True

    def test_defaults(self):
        test = ReadOnlyDefaultDict(bool, {'foo': 1})

        self.assertEqual(test['foo'], 1)

        self.assertEqual(test['qux'], False)


class TestList(unittest.TestCase):
    def test_add_list(self):
        test = List([1, 2, 3])

        test += [4, 5, 6]
        self.assertIsInstance(test, List)
        self.assertEqual(test, [1, 2, 3, 4, 5, 6])

        test = test + [7, 8]
        self.assertIsInstance(test, List)
        self.assertEqual(test, [1, 2, 3, 4, 5, 6, 7, 8])

    def test_add_string(self):
        test = List([1, 2, 3])

        with self.assertRaises(ValueError):
            test += 'string'

    def test_none(self):
        """As a special exception, we allow None to be treated as an empty
        list."""
        test = List([1, 2, 3])

        test += None
        self.assertEqual(test, [1, 2, 3])

        test = test + None
        self.assertIsInstance(test, List)
        self.assertEqual(test, [1, 2, 3])

        with self.assertRaises(ValueError):
            test += False

        with self.assertRaises(ValueError):
            test = test + False

if __name__ == '__main__':
    main()
