# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
from __future__ import unicode_literals

from collections import namedtuple
import unittest

from compare_locales.keyedtuple import KeyedTuple


KeyedThing = namedtuple('KeyedThing', ['key', 'val'])


class TestKeyedTuple(unittest.TestCase):
    def test_constructor(self):
        keyedtuple = KeyedTuple([])
        self.assertEqual(keyedtuple, tuple())

    def test_contains(self):
        things = [KeyedThing('one', 'thing'), KeyedThing('two', 'things')]
        keyedtuple = KeyedTuple(things)
        self.assertNotIn(1, keyedtuple)
        self.assertIn('one', keyedtuple)
        self.assertIn(things[0], keyedtuple)
        self.assertIn(things[1], keyedtuple)
        self.assertNotIn(KeyedThing('three', 'stooges'), keyedtuple)

    def test_getitem(self):
        things = [KeyedThing('one', 'thing'), KeyedThing('two', 'things')]
        keyedtuple = KeyedTuple(things)
        self.assertEqual(keyedtuple[0], things[0])
        self.assertEqual(keyedtuple[1], things[1])
        self.assertEqual(keyedtuple['one'], things[0])
        self.assertEqual(keyedtuple['two'], things[1])

    def test_items(self):
        things = [KeyedThing('one', 'thing'), KeyedThing('two', 'things')]
        things.extend(things)
        keyedtuple = KeyedTuple(things)
        self.assertEqual(len(keyedtuple), 4)
        items = list(keyedtuple.items())
        self.assertEqual(len(items), 4)
        self.assertEqual(
            keyedtuple,
            tuple((v for k, v in items))
        )
        self.assertEqual(
            ('one', 'two', 'one', 'two',),
            tuple((k for k, v in items))
        )
