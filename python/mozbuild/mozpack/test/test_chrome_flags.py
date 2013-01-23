# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest
import mozunit
from mozpack.chrome.flags import (
    Flag,
    StringFlag,
    VersionFlag,
    Flags,
)
from mozpack.errors import ErrorMessage


class TestFlag(unittest.TestCase):
    def test_flag(self):
        flag = Flag('flag')
        self.assertEqual(str(flag), '')
        self.assertTrue(flag.matches(False))
        self.assertTrue(flag.matches('false'))
        self.assertFalse(flag.matches('true'))
        self.assertRaises(ErrorMessage, flag.add_definition, 'flag=')
        self.assertRaises(ErrorMessage, flag.add_definition, 'flag=42')
        self.assertRaises(ErrorMessage, flag.add_definition, 'flag!=false')

        flag.add_definition('flag=1')
        self.assertEqual(str(flag), 'flag=1')
        self.assertTrue(flag.matches(True))
        self.assertTrue(flag.matches('1'))
        self.assertFalse(flag.matches('no'))

        flag.add_definition('flag=true')
        self.assertEqual(str(flag), 'flag=true')
        self.assertTrue(flag.matches(True))
        self.assertTrue(flag.matches('true'))
        self.assertFalse(flag.matches('0'))

        flag.add_definition('flag=no')
        self.assertEqual(str(flag), 'flag=no')
        self.assertTrue(flag.matches('false'))
        self.assertFalse(flag.matches('1'))

        flag.add_definition('flag')
        self.assertEqual(str(flag), 'flag')
        self.assertFalse(flag.matches('false'))
        self.assertTrue(flag.matches('true'))
        self.assertFalse(flag.matches(False))

    def test_string_flag(self):
        flag = StringFlag('flag')
        self.assertEqual(str(flag), '')
        self.assertTrue(flag.matches('foo'))
        self.assertRaises(ErrorMessage, flag.add_definition, 'flag>=2')

        flag.add_definition('flag=foo')
        self.assertEqual(str(flag), 'flag=foo')
        self.assertTrue(flag.matches('foo'))
        self.assertFalse(flag.matches('bar'))

        flag.add_definition('flag=bar')
        self.assertEqual(str(flag), 'flag=foo flag=bar')
        self.assertTrue(flag.matches('foo'))
        self.assertTrue(flag.matches('bar'))
        self.assertFalse(flag.matches('baz'))

        flag = StringFlag('flag')
        flag.add_definition('flag!=bar')
        self.assertEqual(str(flag), 'flag!=bar')
        self.assertTrue(flag.matches('foo'))
        self.assertFalse(flag.matches('bar'))

    def test_version_flag(self):
        flag = VersionFlag('flag')
        self.assertEqual(str(flag), '')
        self.assertTrue(flag.matches('1.0'))
        self.assertRaises(ErrorMessage, flag.add_definition, 'flag!=2')

        flag.add_definition('flag=1.0')
        self.assertEqual(str(flag), 'flag=1.0')
        self.assertTrue(flag.matches('1.0'))
        self.assertFalse(flag.matches('2.0'))

        flag.add_definition('flag=2.0')
        self.assertEqual(str(flag), 'flag=1.0 flag=2.0')
        self.assertTrue(flag.matches('1.0'))
        self.assertTrue(flag.matches('2.0'))
        self.assertFalse(flag.matches('3.0'))

        flag = VersionFlag('flag')
        flag.add_definition('flag>=2.0')
        self.assertEqual(str(flag), 'flag>=2.0')
        self.assertFalse(flag.matches('1.0'))
        self.assertTrue(flag.matches('2.0'))
        self.assertTrue(flag.matches('3.0'))

        flag.add_definition('flag<1.10')
        self.assertEqual(str(flag), 'flag>=2.0 flag<1.10')
        self.assertTrue(flag.matches('1.0'))
        self.assertTrue(flag.matches('1.9'))
        self.assertFalse(flag.matches('1.10'))
        self.assertFalse(flag.matches('1.20'))
        self.assertTrue(flag.matches('2.0'))
        self.assertTrue(flag.matches('3.0'))
        self.assertRaises(Exception, flag.add_definition, 'flag<')
        self.assertRaises(Exception, flag.add_definition, 'flag>')
        self.assertRaises(Exception, flag.add_definition, 'flag>=')
        self.assertRaises(Exception, flag.add_definition, 'flag<=')
        self.assertRaises(Exception, flag.add_definition, 'flag!=1.0')


class TestFlags(unittest.TestCase):
    def setUp(self):
        self.flags = Flags('contentaccessible=yes',
                           'appversion>=3.5',
                           'application=foo',
                           'application=bar',
                           'appversion<2.0',
                           'platform',
                           'abi!=Linux_x86-gcc3')

    def test_flags_str(self):
        self.assertEqual(str(self.flags), 'contentaccessible=yes ' +
                         'appversion>=3.5 appversion<2.0 application=foo ' +
                         'application=bar platform abi!=Linux_x86-gcc3')

    def test_flags_match_unset(self):
        self.assertTrue(self.flags.match(os='WINNT'))

    def test_flags_match(self):
        self.assertTrue(self.flags.match(application='foo'))
        self.assertFalse(self.flags.match(application='qux'))

    def test_flags_match_different(self):
        self.assertTrue(self.flags.match(abi='WINNT_x86-MSVC'))
        self.assertFalse(self.flags.match(abi='Linux_x86-gcc3'))

    def test_flags_match_version(self):
        self.assertTrue(self.flags.match(appversion='1.0'))
        self.assertTrue(self.flags.match(appversion='1.5'))
        self.assertFalse(self.flags.match(appversion='2.0'))
        self.assertFalse(self.flags.match(appversion='3.0'))
        self.assertTrue(self.flags.match(appversion='3.5'))
        self.assertTrue(self.flags.match(appversion='3.10'))


if __name__ == '__main__':
    mozunit.main()
