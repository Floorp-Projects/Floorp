# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import unittest

from compare_locales.lint import linter
from compare_locales.parser import base as parser


class MockChecker(object):
    def __init__(self, mocked):
        self.results = mocked

    def check(self, ent, ref):
        for r in self.results:
            yield r


class EntityTest(unittest.TestCase):
    def test_junk(self):
        el = linter.EntityLinter([], None, {})
        ctx = parser.Parser.Context('foo\nbar\n')
        ent = parser.Junk(ctx, (4, 7))
        res = el.handle_junk(ent)
        self.assertIsNotNone(res)
        self.assertEqual(res['lineno'], 2)
        self.assertEqual(res['column'], 1)
        ent = parser.LiteralEntity('one', 'two', 'one = two')
        self.assertIsNone(el.handle_junk(ent))

    def test_full_entity(self):
        ctx = parser.Parser.Context('''\
one = two
two = three
one = four
''')
        entities = [
            parser.Entity(ctx, None, None, (0, 10), (0, 3), (6, 9)),
            parser.Entity(ctx, None, None, (10, 22), (10, 13), (16, 21)),
            parser.Entity(ctx, None, None, (22, 33), (22, 25), (28, 32)),
        ]
        self.assertEqual(
            (entities[0].all, entities[0].key, entities[0].val),
            ('one = two\n', 'one', 'two')
        )
        self.assertEqual(
            (entities[1].all, entities[1].key, entities[1].val),
            ('two = three\n', 'two', 'three')
        )
        self.assertEqual(
            (entities[2].all, entities[2].key, entities[2].val),
            ('one = four\n', 'one', 'four')
        )
        el = linter.EntityLinter(entities, None, {})
        results = list(el.lint_full_entity(entities[1]))
        self.assertListEqual(results, [])
        results = list(el.lint_full_entity(entities[2]))
        self.assertEqual(len(results), 1)
        result = results[0]
        self.assertEqual(result['level'], 'error')
        self.assertEqual(result['lineno'], 3)
        self.assertEqual(result['column'], 1)
        # finally check for conflict
        el.reference = {
            'two': parser.LiteralEntity('two = other', 'two', 'other')
        }
        results = list(el.lint_full_entity(entities[1]))
        self.assertEqual(len(results), 1)
        result = results[0]
        self.assertEqual(result['level'], 'warning')
        self.assertEqual(result['lineno'], 2)
        self.assertEqual(result['column'], 1)

    def test_in_value(self):
        ctx = parser.Parser.Context('''\
one = two
''')
        entities = [
            parser.Entity(ctx, None, None, (0, 10), (0, 3), (6, 9)),
        ]
        self.assertEqual(
            (entities[0].all, entities[0].key, entities[0].val),
            ('one = two\n', 'one', 'two')
        )
        checker = MockChecker([
            ('error', 2, 'Incompatible resource types', 'android'),
        ])
        el = linter.EntityLinter(entities, checker, {})
        results = list(el.lint_value(entities[0]))
        self.assertEqual(len(results), 1)
        result = results[0]
        self.assertEqual(result['level'], 'error')
        self.assertEqual(result['lineno'], 1)
        self.assertEqual(result['column'], 9)
