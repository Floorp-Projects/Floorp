# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest

from mozunit import main

from mozbuild.util import (
    DefaultOnReadDict,
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

class TestDefaultOnReadDict(unittest.TestCase):
    def test_no_defaults(self):
        original = {'foo': 1, 'bar': 2}

        test = DefaultOnReadDict(original)
        self.assertEqual(original, test)

        with self.assertRaises(KeyError):
            value = test['missing']

        test['foo'] = 5
        self.assertEqual(test['foo'], 5)

    def test_dict_defaults(self):
        original = {'foo': 1, 'bar': 2}

        test = DefaultOnReadDict(original, defaults={'baz': 3})

        self.assertEqual(original, test)
        self.assertEqual(test['baz'], 3)

        with self.assertRaises(KeyError):
            value = test['missing']

        test['baz'] = 4
        self.assertEqual(test['baz'], 4)

    def test_global_default(self):
        original = {'foo': 1}

        test = DefaultOnReadDict(original, defaults={'bar': 2},
            global_default=10)

        self.assertEqual(original, test)
        self.assertEqual(test['foo'], 1)

        self.assertEqual(test['bar'], 2)
        self.assertEqual(test['baz'], 10)

        test['bar'] = 3
        test['baz'] = 12
        test['other'] = 11

        self.assertEqual(test['bar'], 3)
        self.assertEqual(test['baz'], 12)
        self.assertEqual(test['other'], 11)


class TestReadOnlyDefaultDict(unittest.TestCase):
    def test_simple(self):
        original = {'foo': 1, 'bar': 2}

        test = ReadOnlyDefaultDict(original)

        self.assertEqual(original, test)

        self.assertEqual(test['foo'], 1)

        with self.assertRaises(KeyError):
            value = test['missing']

    def test_assignment(self):
        test = ReadOnlyDefaultDict({})

        with self.assertRaises(Exception):
            test['foo'] = True

    def test_defaults(self):
        test = ReadOnlyDefaultDict({}, defaults={'foo': 1})

        self.assertEqual(test['foo'], 1)


if __name__ == '__main__':
    main()
