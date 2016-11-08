# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest
from mozunit import main
from taskgraph.transforms.base import (
    validate_schema,
    get_keyed_by,
    TransformSequence
)
from voluptuous import Schema

schema = Schema({
    'x': int,
    'y': basestring,
})

transforms = TransformSequence()


@transforms.add
def trans1(config, tests):
    for test in tests:
        test['one'] = 1
        yield test


@transforms.add
def trans2(config, tests):
    for test in tests:
        test['two'] = 2
        yield test


class TestTransformSequence(unittest.TestCase):

    def test_sequence(self):
        tests = [{}, {'two': 1, 'second': True}]
        res = list(transforms({}, tests))
        self.assertEqual(res, [
            {u'two': 2, u'one': 1},
            {u'second': True, u'two': 2, u'one': 1},
        ])


class TestValidateSchema(unittest.TestCase):

    def test_valid(self):
        validate_schema(schema, {'x': 10, 'y': 'foo'}, "pfx")

    def test_invalid(self):
        try:
            validate_schema(schema, {'x': 'not-int'}, "pfx")
            self.fail("no exception raised")
        except Exception, e:
            self.failUnless(str(e).startswith("pfx\n"))


class TestKeyedBy(unittest.TestCase):

    def test_simple_value(self):
        test = {
            'test-name': 'tname',
            'option': 10,
        }
        self.assertEqual(get_keyed_by(test, 'option', 'x'), 10)

    def test_by_value(self):
        test = {
            'test-name': 'tname',
            'option': {
                'by-other-value': {
                    'a': 10,
                    'b': 20,
                },
            },
            'other-value': 'b',
        }
        self.assertEqual(get_keyed_by(test, 'option', 'x'), 20)

    def test_by_value_regex(self):
        test = {
            'test-name': 'tname',
            'option': {
                'by-test-platform': {
                    'macosx64/.*': 10,
                    'linux64/debug': 20,
                    'default': 5,
                },
            },
            'test-platform': 'macosx64/debug',
        }
        self.assertEqual(get_keyed_by(test, 'option', 'x'), 10)

    def test_by_value_default(self):
        test = {
            'test-name': 'tname',
            'option': {
                'by-other-value': {
                    'a': 10,
                    'default': 30,
                },
            },
            'other-value': 'xxx',
        }
        self.assertEqual(get_keyed_by(test, 'option', 'x'), 30)

    def test_by_value_invalid_dict(self):
        test = {
            'test-name': 'tname',
            'option': {
                'by-something-else': {},
                'by-other-value': {},
            },
        }
        self.assertRaises(Exception, get_keyed_by, test, 'option', 'x')

    def test_by_value_invalid_no_default(self):
        test = {
            'test-name': 'tname',
            'option': {
                'by-other-value': {
                    'a': 10,
                },
            },
            'other-value': 'b',
        }
        self.assertRaises(Exception, get_keyed_by, test, 'option', 'x')

    def test_by_value_invalid_no_by(self):
        test = {
            'test-name': 'tname',
            'option': {
                'other-value': {},
            },
        }
        self.assertRaises(Exception, get_keyed_by, test, 'option', 'x')

if __name__ == '__main__':
    main()
