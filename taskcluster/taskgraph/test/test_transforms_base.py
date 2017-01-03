# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest
from mozunit import main
from taskgraph.transforms.base import (
    validate_schema,
    resolve_keyed_by,
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


class TestResolveKeyedBy(unittest.TestCase):

    def test_no_by(self):
        self.assertEqual(
            resolve_keyed_by({'x': 10}, 'z', 'n'),
            {'x': 10})

    def test_no_by_dotted(self):
        self.assertEqual(
            resolve_keyed_by({'x': {'y': 10}}, 'x.z', 'n'),
            {'x': {'y': 10}})

    def test_no_by_not_dict(self):
        self.assertEqual(
            resolve_keyed_by({'x': 10}, 'x.y', 'n'),
            {'x': 10})

    def test_no_by_not_by(self):
        self.assertEqual(
            resolve_keyed_by({'x': {'a': 10}}, 'x', 'n'),
            {'x': {'a': 10}})

    def test_no_by_empty_dict(self):
        self.assertEqual(
            resolve_keyed_by({'x': {}}, 'x', 'n'),
            {'x': {}})

    def test_no_by_not_only_by(self):
        self.assertEqual(
            resolve_keyed_by({'x': {'by-y': True, 'a': 10}}, 'x', 'n'),
            {'x': {'by-y': True, 'a': 10}})

    def test_match_nested_exact(self):
        self.assertEqual(
            resolve_keyed_by(
                {'f': 'shoes', 'x': {'y': {'by-f': {'shoes': 'feet', 'gloves': 'hands'}}}},
                'x.y', 'n'),
            {'f': 'shoes', 'x': {'y': 'feet'}})

    def test_match_regexp(self):
        self.assertEqual(
            resolve_keyed_by(
                {'f': 'shoes', 'x': {'by-f': {'s?[hH]oes?': 'feet', 'gloves': 'hands'}}},
                'x', 'n'),
            {'f': 'shoes', 'x': 'feet'})

    def test_match_partial_regexp(self):
        self.assertEqual(
            resolve_keyed_by(
                {'f': 'shoes', 'x': {'by-f': {'sh': 'feet', 'default': 'hands'}}},
                'x', 'n'),
            {'f': 'shoes', 'x': 'hands'})

    def test_match_default(self):
        self.assertEqual(
            resolve_keyed_by(
                {'f': 'shoes', 'x': {'by-f': {'hat': 'head', 'default': 'anywhere'}}},
                'x', 'n'),
            {'f': 'shoes', 'x': 'anywhere'})

    def test_no_match(self):
        self.assertRaises(
            Exception, resolve_keyed_by,
            {'f': 'shoes', 'x': {'by-f': {'hat': 'head'}}}, 'x', 'n')

    def test_multiple_matches(self):
        self.assertRaises(
            Exception, resolve_keyed_by,
            {'f': 'hats', 'x': {'by-f': {'hat.*': 'head', 'ha.*': 'hair'}}}, 'x', 'n')


if __name__ == '__main__':
    main()
