# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import unittest

from mozunit import main

from mozbuild.frontend.context import (
    Context,
    VARIABLES,
)


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
        self.assertIn('DIRS', VARIABLES)

        ns = Context(allowed_variables=VARIABLES)

        ns['DIRS'] = ['foo']
        self.assertEqual(ns['DIRS'], ['foo'])

    def test_value_checking(self):
        ns = Context(allowed_variables=VARIABLES)

        # Setting to a non-allowed type should not work.
        with self.assertRaises(ValueError) as ve:
            ns['DIRS'] = True

        e = ve.exception.args
        self.assertEqual(e[0], 'global_ns')
        self.assertEqual(e[1], 'set_type')
        self.assertEqual(e[2], 'DIRS')
        self.assertTrue(e[3])
        self.assertEqual(e[4], list)

    def test_key_checking(self):
        # Checking for existence of a key should not populate the key if it
        # doesn't exist.
        g = Context(allowed_variables=VARIABLES)

        self.assertFalse('DIRS' in g)
        self.assertFalse('DIRS' in g)


if __name__ == '__main__':
    main()
