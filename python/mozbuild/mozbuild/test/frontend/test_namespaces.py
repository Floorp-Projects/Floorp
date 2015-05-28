# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import unittest

from mozunit import main

from mozbuild.frontend.context import (
    Context,
    ContextDerivedValue,
    ContextDerivedTypedList,
    ContextDerivedTypedListWithItems,
)

from mozbuild.util import (
    StrictOrderingOnAppendList,
    StrictOrderingOnAppendListWithFlagsFactory,
    UnsortedError,
)


class Fuga(object):
    def __init__(self, value):
        self.value = value


class Piyo(ContextDerivedValue):
    def __init__(self, context, value):
        if not isinstance(value, unicode):
            raise ValueError
        self.context = context
        self.value = value

    def lower(self):
        return self.value.lower()

    def __str__(self):
        return self.value

    def __cmp__(self, other):
        return cmp(self.value, str(other))

    def __hash__(self):
        return hash(self.value)


VARIABLES = {
    'HOGE': (unicode, unicode, None, None),
    'FUGA': (Fuga, unicode, None, None),
    'PIYO': (Piyo, unicode, None, None),
    'HOGERA': (ContextDerivedTypedList(Piyo, StrictOrderingOnAppendList),
        list, None, None),
    'HOGEHOGE': (ContextDerivedTypedListWithItems(
        Piyo,
        StrictOrderingOnAppendListWithFlagsFactory({
            'foo': bool,
        })), list, None, None),
}

class TestContext(unittest.TestCase):
    def test_key_rejection(self):
        # Lowercase keys should be rejected during normal operation.
        ns = Context(allowed_variables=VARIABLES)

        with self.assertRaises(KeyError) as ke:
            ns['foo'] = True

        e = ke.exception.args
        self.assertEqual(e[0], 'global_ns')
        self.assertEqual(e[1], 'set_unknown')
        self.assertEqual(e[2], 'foo')
        self.assertTrue(e[3])

        # Unknown uppercase keys should be rejected.
        with self.assertRaises(KeyError) as ke:
            ns['FOO'] = True

        e = ke.exception.args
        self.assertEqual(e[0], 'global_ns')
        self.assertEqual(e[1], 'set_unknown')
        self.assertEqual(e[2], 'FOO')
        self.assertTrue(e[3])

    def test_allowed_set(self):
        self.assertIn('HOGE', VARIABLES)

        ns = Context(allowed_variables=VARIABLES)

        ns['HOGE'] = 'foo'
        self.assertEqual(ns['HOGE'], 'foo')

    def test_value_checking(self):
        ns = Context(allowed_variables=VARIABLES)

        # Setting to a non-allowed type should not work.
        with self.assertRaises(ValueError) as ve:
            ns['HOGE'] = True

        e = ve.exception.args
        self.assertEqual(e[0], 'global_ns')
        self.assertEqual(e[1], 'set_type')
        self.assertEqual(e[2], 'HOGE')
        self.assertEqual(e[3], True)
        self.assertEqual(e[4], unicode)

    def test_key_checking(self):
        # Checking for existence of a key should not populate the key if it
        # doesn't exist.
        g = Context(allowed_variables=VARIABLES)

        self.assertFalse('HOGE' in g)
        self.assertFalse('HOGE' in g)

    def test_coercion(self):
        ns = Context(allowed_variables=VARIABLES)

        # Setting to a type different from the allowed input type should not
        # work.
        with self.assertRaises(ValueError) as ve:
            ns['FUGA'] = False

        e = ve.exception.args
        self.assertEqual(e[0], 'global_ns')
        self.assertEqual(e[1], 'set_type')
        self.assertEqual(e[2], 'FUGA')
        self.assertEqual(e[3], False)
        self.assertEqual(e[4], unicode)

        ns['FUGA'] = 'fuga'
        self.assertIsInstance(ns['FUGA'], Fuga)
        self.assertEqual(ns['FUGA'].value, 'fuga')

        ns['FUGA'] = Fuga('hoge')
        self.assertIsInstance(ns['FUGA'], Fuga)
        self.assertEqual(ns['FUGA'].value, 'hoge')

    def test_context_derived_coercion(self):
        ns = Context(allowed_variables=VARIABLES)

        # Setting to a type different from the allowed input type should not
        # work.
        with self.assertRaises(ValueError) as ve:
            ns['PIYO'] = False

        e = ve.exception.args
        self.assertEqual(e[0], 'global_ns')
        self.assertEqual(e[1], 'set_type')
        self.assertEqual(e[2], 'PIYO')
        self.assertEqual(e[3], False)
        self.assertEqual(e[4], unicode)

        ns['PIYO'] = 'piyo'
        self.assertIsInstance(ns['PIYO'], Piyo)
        self.assertEqual(ns['PIYO'].value, 'piyo')
        self.assertEqual(ns['PIYO'].context, ns)

        ns['PIYO'] = Piyo(ns, 'fuga')
        self.assertIsInstance(ns['PIYO'], Piyo)
        self.assertEqual(ns['PIYO'].value, 'fuga')
        self.assertEqual(ns['PIYO'].context, ns)

    def test_context_derived_typed_list(self):
        ns = Context(allowed_variables=VARIABLES)

        # Setting to a type that's rejected by coercion should not work.
        with self.assertRaises(ValueError):
            ns['HOGERA'] = [False]

        ns['HOGERA'] += ['a', 'b', 'c']

        self.assertIsInstance(ns['HOGERA'], VARIABLES['HOGERA'][0])
        for n in range(0, 3):
            self.assertIsInstance(ns['HOGERA'][n], Piyo)
            self.assertEqual(ns['HOGERA'][n].value, ['a', 'b', 'c'][n])
            self.assertEqual(ns['HOGERA'][n].context, ns)

        with self.assertRaises(UnsortedError):
            ns['HOGERA'] += ['f', 'e', 'd']

    def test_context_derived_typed_list_with_items(self):
        ns = Context(allowed_variables=VARIABLES)

        # Setting to a type that's rejected by coercion should not work.
        with self.assertRaises(ValueError):
            ns['HOGEHOGE'] = [False]

        values = ['a', 'b', 'c']
        ns['HOGEHOGE'] += values

        self.assertIsInstance(ns['HOGEHOGE'], VARIABLES['HOGEHOGE'][0])
        for v in values:
            ns['HOGEHOGE'][v].foo = True

        for v, item in zip(values, ns['HOGEHOGE']):
            self.assertIsInstance(item, Piyo)
            self.assertEqual(v, item)
            self.assertEqual(ns['HOGEHOGE'][v].foo, True)
            self.assertEqual(ns['HOGEHOGE'][item].foo, True)

        with self.assertRaises(UnsortedError):
            ns['HOGEHOGE'] += ['f', 'e', 'd']

if __name__ == '__main__':
    main()
