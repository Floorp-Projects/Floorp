# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import unicode_literals

import sys
import unittest

from mozfile.mozfile import NamedTemporaryFile

from mach.config import (
    BooleanType,
    ConfigException,
    ConfigSettings,
    IntegerType,
    PathType,
    PositiveIntegerType,
    StringType,
)
from mach.decorators import SettingsProvider

from mozunit import main


if sys.version_info[0] == 3:
    str_type = str
else:
    str_type = basestring

CONFIG1 = r"""
[foo]

bar = bar_value
baz = /baz/foo.c
"""

CONFIG2 = r"""
[foo]

bar = value2
"""

@SettingsProvider
class Provider1(object):
    config_settings = [
        ('foo.bar', StringType),
        ('foo.baz', PathType),
    ]


@SettingsProvider
class ProviderDuplicate(object):
    config_settings = [
        ('dupesect.foo', StringType),
        ('dupesect.foo', StringType),
    ]


@SettingsProvider
class Provider2(object):
    config_settings = [
        ('a.string', StringType),
        ('a.boolean', BooleanType),
        ('a.pos_int', PositiveIntegerType),
        ('a.int', IntegerType),
        ('a.path', PathType),
    ]


@SettingsProvider
class Provider3(object):
    @classmethod
    def config_settings(cls):
        return [
            ('a.string', 'string'),
            ('a.boolean', 'boolean'),
            ('a.pos_int', 'pos_int'),
            ('a.int', 'int'),
            ('a.path', 'path'),
        ]


@SettingsProvider
class Provider4(object):
    config_settings = [
        ('foo.abc', StringType, 'a', {'choices': set('abc')}),
        ('foo.xyz', StringType, 'w', {'choices': set('xyz')}),
    ]


@SettingsProvider
class Provider5(object):
    config_settings = [
        ('foo.*', 'string'),
        ('foo.bar', 'string'),
    ]


class TestConfigSettings(unittest.TestCase):
    def test_empty(self):
        s = ConfigSettings()

        self.assertEqual(len(s), 0)
        self.assertNotIn('foo', s)

    def test_duplicate_option(self):
        s = ConfigSettings()

        with self.assertRaises(ConfigException):
            s.register_provider(ProviderDuplicate)

    def test_simple(self):
        s = ConfigSettings()
        s.register_provider(Provider1)

        self.assertEqual(len(s), 1)
        self.assertIn('foo', s)

        foo = s['foo']
        foo = s.foo

        self.assertEqual(len(foo), 0)
        self.assertEqual(len(foo._settings), 2)

        self.assertIn('bar', foo._settings)
        self.assertIn('baz', foo._settings)

        self.assertNotIn('bar', foo)
        foo['bar'] = 'value1'
        self.assertIn('bar', foo)

        self.assertEqual(foo['bar'], 'value1')
        self.assertEqual(foo.bar, 'value1')

    def test_assignment_validation(self):
        s = ConfigSettings()
        s.register_provider(Provider2)

        a = s.a

        # Assigning an undeclared setting raises.
        with self.assertRaises(AttributeError):
            a.undefined = True

        with self.assertRaises(KeyError):
            a['undefined'] = True

        # Basic type validation.
        a.string = 'foo'
        a.string = 'foo'

        with self.assertRaises(TypeError):
            a.string = False

        a.boolean = True
        a.boolean = False

        with self.assertRaises(TypeError):
            a.boolean = 'foo'

        a.pos_int = 5
        a.pos_int = 0

        with self.assertRaises(ValueError):
            a.pos_int = -1

        with self.assertRaises(TypeError):
            a.pos_int = 'foo'

        a.int = 5
        a.int = 0
        a.int = -5

        with self.assertRaises(TypeError):
            a.int = 1.24

        with self.assertRaises(TypeError):
            a.int = 'foo'

        a.path = '/home/gps'
        a.path = 'foo.c'
        a.path = 'foo/bar'
        a.path = './foo'

    def retrieval_type_helper(self, provider):
        s = ConfigSettings()
        s.register_provider(provider)

        a = s.a

        a.string = 'foo'
        a.boolean = True
        a.pos_int = 12
        a.int = -4
        a.path = './foo/bar'

        self.assertIsInstance(a.string, str_type)
        self.assertIsInstance(a.boolean, bool)
        self.assertIsInstance(a.pos_int, int)
        self.assertIsInstance(a.int, int)
        self.assertIsInstance(a.path, str_type)

    def test_retrieval_type(self):
        self.retrieval_type_helper(Provider2)
        self.retrieval_type_helper(Provider3)

    def test_choices_validation(self):
        s = ConfigSettings()
        s.register_provider(Provider4)

        foo = s.foo
        foo.abc
        with self.assertRaises(ValueError):
            foo.xyz

        with self.assertRaises(ValueError):
            foo.abc = 'e'

        foo.abc = 'b'
        foo.xyz = 'y'

    def test_wildcard_options(self):
        s = ConfigSettings()
        s.register_provider(Provider5)

        foo = s.foo

        self.assertIn('*', foo._settings)
        self.assertNotIn('*', foo)

        foo.baz = 'value1'
        foo.bar = 'value2'

        self.assertIn('baz', foo)
        self.assertEqual(foo.baz, 'value1')

        self.assertIn('bar', foo)
        self.assertEqual(foo.bar, 'value2')

    def test_file_reading_single(self):
        temp = NamedTemporaryFile(mode='wt')
        temp.write(CONFIG1)
        temp.flush()

        s = ConfigSettings()
        s.register_provider(Provider1)

        s.load_file(temp.name)

        self.assertEqual(s.foo.bar, 'bar_value')

    def test_file_reading_multiple(self):
        """Loading multiple files has proper overwrite behavior."""
        temp1 = NamedTemporaryFile(mode='wt')
        temp1.write(CONFIG1)
        temp1.flush()

        temp2 = NamedTemporaryFile(mode='wt')
        temp2.write(CONFIG2)
        temp2.flush()

        s = ConfigSettings()
        s.register_provider(Provider1)

        s.load_files([temp1.name, temp2.name])

        self.assertEqual(s.foo.bar, 'value2')

    def test_file_reading_missing(self):
        """Missing files should silently be ignored."""

        s = ConfigSettings()

        s.load_file('/tmp/foo.ini')

    def test_file_writing(self):
        s = ConfigSettings()
        s.register_provider(Provider2)

        s.a.string = 'foo'
        s.a.boolean = False

        temp = NamedTemporaryFile('wt')
        s.write(temp)
        temp.flush()

        s2 = ConfigSettings()
        s2.register_provider(Provider2)

        s2.load_file(temp.name)

        self.assertEqual(s.a.string, s2.a.string)
        self.assertEqual(s.a.boolean, s2.a.boolean)


if __name__ == '__main__':
    main()
