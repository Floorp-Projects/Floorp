# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest

from mozunit import main

from mozbuild.frontend.context import (
    Context,
    FUNCTIONS,
    SPECIAL_VARIABLES,
    VARIABLES,
)


class TestContext(unittest.TestCase):
    def test_defaults(self):
        test = Context({
            'foo': (int, int, '', None),
            'bar': (bool, bool, '', None),
            'baz': (dict, dict, '', None),
        })

        self.assertEqual(test.keys(), [])

        self.assertEqual(test['foo'], 0)

        self.assertEqual(set(test.keys()), { 'foo' })

        self.assertEqual(test['bar'], False)

        self.assertEqual(set(test.keys()), { 'foo', 'bar' })

        self.assertEqual(test['baz'], {})

        self.assertEqual(set(test.keys()), { 'foo', 'bar', 'baz' })

        with self.assertRaises(KeyError):
            test['qux']

        self.assertEqual(set(test.keys()), { 'foo', 'bar', 'baz' })

    def test_type_check(self):
        test = Context({
            'foo': (int, int, '', None),
            'baz': (dict, list, '', None),
        })

        test['foo'] = 5

        self.assertEqual(test['foo'], 5)

        with self.assertRaises(ValueError):
            test['foo'] = {}

        self.assertEqual(test['foo'], 5)

        with self.assertRaises(KeyError):
            test['bar'] = True

        test['baz'] = [('a', 1), ('b', 2)]

        self.assertEqual(test['baz'], { 'a': 1, 'b': 2 })

    def test_update(self):
        test = Context({
            'foo': (int, int, '', None),
            'bar': (bool, bool, '', None),
            'baz': (dict, list, '', None),
        })

        self.assertEqual(test.keys(), [])

        with self.assertRaises(ValueError):
            test.update(bar=True, foo={})

        self.assertEqual(test.keys(), [])

        test.update(bar=True, foo=1)

        self.assertEqual(set(test.keys()), { 'foo', 'bar' })
        self.assertEqual(test['foo'], 1)
        self.assertEqual(test['bar'], True)

        test.update([('bar', False), ('foo', 2)])
        self.assertEqual(test['foo'], 2)
        self.assertEqual(test['bar'], False)

        test.update([('foo', 0), ('baz', { 'a': 1, 'b': 2 })])
        self.assertEqual(test['foo'], 0)
        self.assertEqual(test['baz'], { 'a': 1, 'b': 2 })

        test.update([('foo', 42), ('baz', [('c', 3), ('d', 4)])])
        self.assertEqual(test['foo'], 42)
        self.assertEqual(test['baz'], { 'c': 3, 'd': 4 })


class TestSymbols(unittest.TestCase):
    def _verify_doc(self, doc):
        # Documentation should be of the format:
        # """SUMMARY LINE
        #
        # EXTRA PARAGRAPHS
        # """

        self.assertNotIn('\r', doc)

        lines = doc.split('\n')

        # No trailing whitespace.
        for line in lines[0:-1]:
            self.assertEqual(line, line.rstrip())

        self.assertGreater(len(lines), 0)
        self.assertGreater(len(lines[0].strip()), 0)

        # Last line should be empty.
        self.assertEqual(lines[-1].strip(), '')

    def test_documentation_formatting(self):
        for typ, inp, doc, tier in VARIABLES.values():
            self._verify_doc(doc)

        for attr, args, doc in FUNCTIONS.values():
            self._verify_doc(doc)

        for func, typ, doc in SPECIAL_VARIABLES.values():
            self._verify_doc(doc)


if __name__ == '__main__':
    main()
