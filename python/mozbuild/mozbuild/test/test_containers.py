# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest

from mozunit import main

from mozbuild.util import (
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


if __name__ == '__main__':
    main()
