# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest

from mozunit import main

from mozbuild.configure.options import (
    CommandLineHelper,
    ConflictingOptionError,
    InvalidOptionError,
    NegativeOptionValue,
    Option,
    OptionValue,
    PositiveOptionValue,
)


class Option(Option):
    def __init__(self, *args, **kwargs):
        kwargs['help'] = 'Dummy help'
        super(Option, self).__init__(*args, **kwargs)


class TestOption(unittest.TestCase):
    def test_option(self):
        option = Option('--option')
        self.assertEquals(option.prefix, '')
        self.assertEquals(option.name, 'option')
        self.assertEquals(option.env, None)
        self.assertFalse(option.default)

        option = Option('--enable-option')
        self.assertEquals(option.prefix, 'enable')
        self.assertEquals(option.name, 'option')
        self.assertEquals(option.env, None)
        self.assertFalse(option.default)

        option = Option('--disable-option')
        self.assertEquals(option.prefix, 'disable')
        self.assertEquals(option.name, 'option')
        self.assertEquals(option.env, None)
        self.assertTrue(option.default)

        option = Option('--with-option')
        self.assertEquals(option.prefix, 'with')
        self.assertEquals(option.name, 'option')
        self.assertEquals(option.env, None)
        self.assertFalse(option.default)

        option = Option('--without-option')
        self.assertEquals(option.prefix, 'without')
        self.assertEquals(option.name, 'option')
        self.assertEquals(option.env, None)
        self.assertTrue(option.default)

        option = Option('--without-option-foo', env='MOZ_OPTION')
        self.assertEquals(option.env, 'MOZ_OPTION')

        option = Option(env='MOZ_OPTION')
        self.assertEquals(option.prefix, '')
        self.assertEquals(option.name, None)
        self.assertEquals(option.env, 'MOZ_OPTION')
        self.assertFalse(option.default)

        with self.assertRaises(InvalidOptionError) as e:
            Option('--option', nargs=0, default=('a',))
        self.assertEquals(e.exception.message,
                          "The given `default` doesn't satisfy `nargs`")

        with self.assertRaises(InvalidOptionError) as e:
            Option('--option', nargs=1, default=())
        self.assertEquals(
            e.exception.message,
            'default must be a bool, a string or a tuple of strings')

        with self.assertRaises(InvalidOptionError) as e:
            Option('--option', nargs=1, default=True)
        self.assertEquals(e.exception.message,
                          "The given `default` doesn't satisfy `nargs`")

        with self.assertRaises(InvalidOptionError) as e:
            Option('--option', nargs=1, default=('a', 'b'))
        self.assertEquals(e.exception.message,
                          "The given `default` doesn't satisfy `nargs`")

        with self.assertRaises(InvalidOptionError) as e:
            Option('--option', nargs=2, default=())
        self.assertEquals(
            e.exception.message,
            'default must be a bool, a string or a tuple of strings')

        with self.assertRaises(InvalidOptionError) as e:
            Option('--option', nargs=2, default=True)
        self.assertEquals(e.exception.message,
                          "The given `default` doesn't satisfy `nargs`")

        with self.assertRaises(InvalidOptionError) as e:
            Option('--option', nargs=2, default=('a',))
        self.assertEquals(e.exception.message,
                          "The given `default` doesn't satisfy `nargs`")

        with self.assertRaises(InvalidOptionError) as e:
            Option('--option', nargs='?', default=('a', 'b'))
        self.assertEquals(e.exception.message,
                          "The given `default` doesn't satisfy `nargs`")

        with self.assertRaises(InvalidOptionError) as e:
            Option('--option', nargs='+', default=())
        self.assertEquals(
            e.exception.message,
            'default must be a bool, a string or a tuple of strings')

        with self.assertRaises(InvalidOptionError) as e:
            Option('--option', nargs='+', default=True)
        self.assertEquals(e.exception.message,
                          "The given `default` doesn't satisfy `nargs`")

        # --disable options with a nargs value that requires at least one
        # argument need to be given a default.
        with self.assertRaises(InvalidOptionError) as e:
            Option('--disable-option', nargs=1)
        self.assertEquals(e.exception.message,
                          "The given `default` doesn't satisfy `nargs`")

        with self.assertRaises(InvalidOptionError) as e:
            Option('--disable-option', nargs='+')
        self.assertEquals(e.exception.message,
                          "The given `default` doesn't satisfy `nargs`")

        # Test nargs inference from default value
        option = Option('--with-foo', default=True)
        self.assertEquals(option.nargs, 0)

        option = Option('--with-foo', default=False)
        self.assertEquals(option.nargs, 0)

        option = Option('--with-foo', default='a')
        self.assertEquals(option.nargs, '?')

        option = Option('--with-foo', default=('a',))
        self.assertEquals(option.nargs, '?')

        option = Option('--with-foo', default=('a', 'b'))
        self.assertEquals(option.nargs, '*')

        option = Option(env='FOO', default=True)
        self.assertEquals(option.nargs, 0)

        option = Option(env='FOO', default=False)
        self.assertEquals(option.nargs, 0)

        option = Option(env='FOO', default='a')
        self.assertEquals(option.nargs, '?')

        option = Option(env='FOO', default=('a',))
        self.assertEquals(option.nargs, '?')

        option = Option(env='FOO', default=('a', 'b'))
        self.assertEquals(option.nargs, '*')

    def test_option_option(self):
        for option in (
            '--option',
            '--enable-option',
            '--disable-option',
            '--with-option',
            '--without-option',
        ):
            self.assertEquals(Option(option).option, option)
            self.assertEquals(Option(option, env='FOO').option, option)

            opt = Option(option, default=False)
            self.assertEquals(opt.option,
                              option.replace('-disable-', '-enable-')
                                    .replace('-without-', '-with-'))

            opt = Option(option, default=True)
            self.assertEquals(opt.option,
                              option.replace('-enable-', '-disable-')
                                    .replace('-with-', '-without-'))

        self.assertEquals(Option(env='FOO').option, 'FOO')

    def test_option_choices(self):
        with self.assertRaises(InvalidOptionError) as e:
            Option('--option', nargs=3, choices=('a', 'b'))
        self.assertEquals(e.exception.message,
                          'Not enough `choices` for `nargs`')

        with self.assertRaises(InvalidOptionError) as e:
            Option('--without-option', nargs=1, choices=('a', 'b'))
        self.assertEquals(e.exception.message,
                          'A `default` must be given along with `choices`')

        with self.assertRaises(InvalidOptionError) as e:
            Option('--without-option', nargs='+', choices=('a', 'b'))
        self.assertEquals(e.exception.message,
                          'A `default` must be given along with `choices`')

        with self.assertRaises(InvalidOptionError) as e:
            Option('--without-option', default='c', choices=('a', 'b'))
        self.assertEquals(e.exception.message,
                          "The `default` value must be one of 'a', 'b'")

        with self.assertRaises(InvalidOptionError) as e:
            Option('--without-option', default=('a', 'c',), choices=('a', 'b'))
        self.assertEquals(e.exception.message,
                          "The `default` value must be one of 'a', 'b'")

        with self.assertRaises(InvalidOptionError) as e:
            Option('--without-option', default=('c',), choices=('a', 'b'))
        self.assertEquals(e.exception.message,
                          "The `default` value must be one of 'a', 'b'")

        option = Option('--with-option', nargs='+', choices=('a', 'b'))
        with self.assertRaises(InvalidOptionError) as e:
            option.get_value('--with-option=c')
        self.assertEquals(e.exception.message, "'c' is not one of 'a', 'b'")

        value = option.get_value('--with-option=b,a')
        self.assertTrue(value)
        self.assertEquals(PositiveOptionValue(('b', 'a')), value)

        option = Option('--without-option', nargs='*', default='a',
                        choices=('a', 'b'))
        with self.assertRaises(InvalidOptionError) as e:
            option.get_value('--with-option=c')
        self.assertEquals(e.exception.message, "'c' is not one of 'a', 'b'")

        value = option.get_value('--with-option=b,a')
        self.assertTrue(value)
        self.assertEquals(PositiveOptionValue(('b', 'a')), value)

        # Test nargs inference from choices
        option = Option('--with-option', choices=('a', 'b'))
        self.assertEqual(option.nargs, 1)

        # Test "relative" values
        option = Option('--with-option', nargs='*', default=('b', 'c'),
                        choices=('a', 'b', 'c', 'd'))

        value = option.get_value('--with-option=+d')
        self.assertEquals(PositiveOptionValue(('b', 'c', 'd')), value)

        value = option.get_value('--with-option=-b')
        self.assertEquals(PositiveOptionValue(('c',)), value)

        value = option.get_value('--with-option=-b,+d')
        self.assertEquals(PositiveOptionValue(('c','d')), value)

        # Adding something that is in the default is fine
        value = option.get_value('--with-option=+b')
        self.assertEquals(PositiveOptionValue(('b', 'c')), value)

        # Removing something that is not in the default is fine, as long as it
        # is one of the choices
        value = option.get_value('--with-option=-a')
        self.assertEquals(PositiveOptionValue(('b', 'c')), value)

        with self.assertRaises(InvalidOptionError) as e:
            option.get_value('--with-option=-e')
        self.assertEquals(e.exception.message,
                          "'e' is not one of 'a', 'b', 'c', 'd'")

        # Other "not a choice" errors.
        with self.assertRaises(InvalidOptionError) as e:
            option.get_value('--with-option=+e')
        self.assertEquals(e.exception.message,
                          "'e' is not one of 'a', 'b', 'c', 'd'")

        with self.assertRaises(InvalidOptionError) as e:
            option.get_value('--with-option=e')
        self.assertEquals(e.exception.message,
                          "'e' is not one of 'a', 'b', 'c', 'd'")

    def test_option_value_compare(self):
        # OptionValue are tuple and equivalence should compare as tuples.
        val = PositiveOptionValue(('foo',))

        self.assertEqual(val[0], 'foo')
        self.assertEqual(val, PositiveOptionValue(('foo',)))
        self.assertNotEqual(val, PositiveOptionValue(('foo', 'bar')))

        # Can compare a tuple to an OptionValue.
        self.assertEqual(val, ('foo',))
        self.assertNotEqual(val, ('foo', 'bar'))

        # Different OptionValue types are never equal.
        self.assertNotEqual(val, OptionValue(('foo',)))

        # For usability reasons, we raise TypeError when attempting to compare
        # against a non-tuple.
        with self.assertRaisesRegexp(TypeError, 'cannot compare a'):
            val == 'foo'

    def test_option_value_format(self):
        val = PositiveOptionValue()
        self.assertEquals('--with-value', val.format('--with-value'))
        self.assertEquals('--with-value', val.format('--without-value'))
        self.assertEquals('--enable-value', val.format('--enable-value'))
        self.assertEquals('--enable-value', val.format('--disable-value'))
        self.assertEquals('--value', val.format('--value'))
        self.assertEquals('VALUE=1', val.format('VALUE'))

        val = PositiveOptionValue(('a',))
        self.assertEquals('--with-value=a', val.format('--with-value'))
        self.assertEquals('--with-value=a', val.format('--without-value'))
        self.assertEquals('--enable-value=a', val.format('--enable-value'))
        self.assertEquals('--enable-value=a', val.format('--disable-value'))
        self.assertEquals('--value=a', val.format('--value'))
        self.assertEquals('VALUE=a', val.format('VALUE'))

        val = PositiveOptionValue(('a', 'b'))
        self.assertEquals('--with-value=a,b', val.format('--with-value'))
        self.assertEquals('--with-value=a,b', val.format('--without-value'))
        self.assertEquals('--enable-value=a,b', val.format('--enable-value'))
        self.assertEquals('--enable-value=a,b', val.format('--disable-value'))
        self.assertEquals('--value=a,b', val.format('--value'))
        self.assertEquals('VALUE=a,b', val.format('VALUE'))

        val = NegativeOptionValue()
        self.assertEquals('--without-value', val.format('--with-value'))
        self.assertEquals('--without-value', val.format('--without-value'))
        self.assertEquals('--disable-value', val.format('--enable-value'))
        self.assertEquals('--disable-value', val.format('--disable-value'))
        self.assertEquals('', val.format('--value'))
        self.assertEquals('VALUE=', val.format('VALUE'))

    def test_option_value(self, name='option', nargs=0, default=None):
        disabled = name.startswith(('disable-', 'without-'))
        if disabled:
            negOptionValue = PositiveOptionValue
            posOptionValue = NegativeOptionValue
        else:
            posOptionValue = PositiveOptionValue
            negOptionValue = NegativeOptionValue
        defaultValue = (PositiveOptionValue(default)
                        if default else negOptionValue())

        option = Option('--%s' % name, nargs=nargs, default=default)

        if nargs in (0, '?', '*') or disabled:
            value = option.get_value('--%s' % name, 'option')
            self.assertEquals(value, posOptionValue())
            self.assertEquals(value.origin, 'option')
        else:
            with self.assertRaises(InvalidOptionError) as e:
                option.get_value('--%s' % name)
            if nargs == 1:
                self.assertEquals(e.exception.message,
                                  '--%s takes 1 value' % name)
            elif nargs == '+':
                self.assertEquals(e.exception.message,
                                  '--%s takes 1 or more values' % name)
            else:
                self.assertEquals(e.exception.message,
                                  '--%s takes 2 values' % name)

        value = option.get_value('')
        self.assertEquals(value, defaultValue)
        self.assertEquals(value.origin, 'default')

        value = option.get_value(None)
        self.assertEquals(value, defaultValue)
        self.assertEquals(value.origin, 'default')

        with self.assertRaises(AssertionError):
            value = option.get_value('MOZ_OPTION=', 'environment')

        with self.assertRaises(AssertionError):
            value = option.get_value('MOZ_OPTION=1', 'environment')

        with self.assertRaises(AssertionError):
            value = option.get_value('--foo')

        if nargs in (1, '?', '*', '+') and not disabled:
            value = option.get_value('--%s=' % name, 'option')
            self.assertEquals(value, PositiveOptionValue(('',)))
            self.assertEquals(value.origin, 'option')
        else:
            with self.assertRaises(InvalidOptionError) as e:
                option.get_value('--%s=' % name)
            if disabled:
                self.assertEquals(e.exception.message,
                                  'Cannot pass a value to --%s' % name)
            else:
                self.assertEquals(e.exception.message,
                                  '--%s takes %d values' % (name, nargs))

        if nargs in (1, '?', '*', '+') and not disabled:
            value = option.get_value('--%s=foo' % name, 'option')
            self.assertEquals(value, PositiveOptionValue(('foo',)))
            self.assertEquals(value.origin, 'option')
        else:
            with self.assertRaises(InvalidOptionError) as e:
                option.get_value('--%s=foo' % name)
            if disabled:
                self.assertEquals(e.exception.message,
                                  'Cannot pass a value to --%s' % name)
            else:
                self.assertEquals(e.exception.message,
                                  '--%s takes %d values' % (name, nargs))

        if nargs in (2, '*', '+') and not disabled:
            value = option.get_value('--%s=foo,bar' % name, 'option')
            self.assertEquals(value, PositiveOptionValue(('foo', 'bar')))
            self.assertEquals(value.origin, 'option')
        else:
            with self.assertRaises(InvalidOptionError) as e:
                option.get_value('--%s=foo,bar' % name, 'option')
            if disabled:
                self.assertEquals(e.exception.message,
                                  'Cannot pass a value to --%s' % name)
            elif nargs == '?':
                self.assertEquals(e.exception.message,
                                  '--%s takes 0 or 1 values' % name)
            else:
                self.assertEquals(e.exception.message,
                                  '--%s takes %d value%s'
                                  % (name, nargs, 's' if nargs != 1 else ''))

        option = Option('--%s' % name, env='MOZ_OPTION', nargs=nargs,
                        default=default)
        if nargs in (0, '?', '*') or disabled:
            value = option.get_value('--%s' % name, 'option')
            self.assertEquals(value, posOptionValue())
            self.assertEquals(value.origin, 'option')
        else:
            with self.assertRaises(InvalidOptionError) as e:
                option.get_value('--%s' % name)
            if disabled:
                self.assertEquals(e.exception.message,
                                  'Cannot pass a value to --%s' % name)
            elif nargs == '+':
                self.assertEquals(e.exception.message,
                                  '--%s takes 1 or more values' % name)
            else:
                self.assertEquals(e.exception.message,
                                  '--%s takes %d value%s'
                                  % (name, nargs, 's' if nargs != 1 else ''))

        value = option.get_value('')
        self.assertEquals(value, defaultValue)
        self.assertEquals(value.origin, 'default')

        value = option.get_value(None)
        self.assertEquals(value, defaultValue)
        self.assertEquals(value.origin, 'default')

        value = option.get_value('MOZ_OPTION=', 'environment')
        self.assertEquals(value, NegativeOptionValue())
        self.assertEquals(value.origin, 'environment')

        if nargs in (0, '?', '*'):
            value = option.get_value('MOZ_OPTION=1', 'environment')
            self.assertEquals(value, PositiveOptionValue())
            self.assertEquals(value.origin, 'environment')
        elif nargs in (1, '+'):
            value = option.get_value('MOZ_OPTION=1', 'environment')
            self.assertEquals(value, PositiveOptionValue(('1',)))
            self.assertEquals(value.origin, 'environment')
        else:
            with self.assertRaises(InvalidOptionError) as e:
                option.get_value('MOZ_OPTION=1', 'environment')
            self.assertEquals(e.exception.message, 'MOZ_OPTION takes 2 values')

        if nargs in (1, '?', '*', '+') and not disabled:
            value = option.get_value('--%s=' % name, 'option')
            self.assertEquals(value, PositiveOptionValue(('',)))
            self.assertEquals(value.origin, 'option')
        else:
            with self.assertRaises(InvalidOptionError) as e:
                option.get_value('--%s=' % name, 'option')
            if disabled:
                self.assertEquals(e.exception.message,
                                  'Cannot pass a value to --%s' % name)
            else:
                self.assertEquals(e.exception.message,
                                  '--%s takes %d values' % (name, nargs))

        with self.assertRaises(AssertionError):
            value = option.get_value('--foo', 'option')

        if nargs in (1, '?', '*', '+'):
            value = option.get_value('MOZ_OPTION=foo', 'environment')
            self.assertEquals(value, PositiveOptionValue(('foo',)))
            self.assertEquals(value.origin, 'environment')
        else:
            with self.assertRaises(InvalidOptionError) as e:
                option.get_value('MOZ_OPTION=foo', 'environment')
            self.assertEquals(e.exception.message,
                              'MOZ_OPTION takes %d values' % nargs)

        if nargs in (2, '*', '+'):
            value = option.get_value('MOZ_OPTION=foo,bar', 'environment')
            self.assertEquals(value, PositiveOptionValue(('foo', 'bar')))
            self.assertEquals(value.origin, 'environment')
        else:
            with self.assertRaises(InvalidOptionError) as e:
                option.get_value('MOZ_OPTION=foo,bar', 'environment')
            if nargs == '?':
                self.assertEquals(e.exception.message,
                                  'MOZ_OPTION takes 0 or 1 values')
            else:
                self.assertEquals(e.exception.message,
                                  'MOZ_OPTION takes %d value%s'
                                  % (nargs, 's' if nargs != 1 else ''))

        if disabled:
            return option

        env_option = Option(env='MOZ_OPTION', nargs=nargs, default=default)
        with self.assertRaises(AssertionError):
            env_option.get_value('--%s' % name)

        value = env_option.get_value('')
        self.assertEquals(value, defaultValue)
        self.assertEquals(value.origin, 'default')

        value = env_option.get_value('MOZ_OPTION=', 'environment')
        self.assertEquals(value, negOptionValue())
        self.assertEquals(value.origin, 'environment')

        if nargs in (0, '?', '*'):
            value = env_option.get_value('MOZ_OPTION=1', 'environment')
            self.assertEquals(value, posOptionValue())
            self.assertTrue(value)
            self.assertEquals(value.origin, 'environment')
        elif nargs in (1, '+'):
            value = env_option.get_value('MOZ_OPTION=1', 'environment')
            self.assertEquals(value, PositiveOptionValue(('1',)))
            self.assertEquals(value.origin, 'environment')
        else:
            with self.assertRaises(InvalidOptionError) as e:
                env_option.get_value('MOZ_OPTION=1', 'environment')
            self.assertEquals(e.exception.message, 'MOZ_OPTION takes 2 values')

        with self.assertRaises(AssertionError) as e:
            env_option.get_value('--%s' % name)

        with self.assertRaises(AssertionError) as e:
            env_option.get_value('--foo')

        if nargs in (1, '?', '*', '+'):
            value = env_option.get_value('MOZ_OPTION=foo', 'environment')
            self.assertEquals(value, PositiveOptionValue(('foo',)))
            self.assertEquals(value.origin, 'environment')
        else:
            with self.assertRaises(InvalidOptionError) as e:
                env_option.get_value('MOZ_OPTION=foo', 'environment')
            self.assertEquals(e.exception.message,
                              'MOZ_OPTION takes %d values' % nargs)

        if nargs in (2, '*', '+'):
            value = env_option.get_value('MOZ_OPTION=foo,bar', 'environment')
            self.assertEquals(value, PositiveOptionValue(('foo', 'bar')))
            self.assertEquals(value.origin, 'environment')
        else:
            with self.assertRaises(InvalidOptionError) as e:
                env_option.get_value('MOZ_OPTION=foo,bar', 'environment')
            if nargs == '?':
                self.assertEquals(e.exception.message,
                                  'MOZ_OPTION takes 0 or 1 values')
            else:
                self.assertEquals(e.exception.message,
                                  'MOZ_OPTION takes %d value%s'
                                  % (nargs, 's' if nargs != 1 else ''))

        return option

    def test_option_value_enable(self, enable='enable', disable='disable',
                                 nargs=0, default=None):
        option = self.test_option_value('%s-option' % enable, nargs=nargs,
                                        default=default)

        value = option.get_value('--%s-option' % disable, 'option')
        self.assertEquals(value, NegativeOptionValue())
        self.assertEquals(value.origin, 'option')

        option = self.test_option_value('%s-option' % disable, nargs=nargs,
                                        default=default)

        if nargs in (0, '?', '*'):
            value = option.get_value('--%s-option' % enable, 'option')
            self.assertEquals(value, PositiveOptionValue())
            self.assertEquals(value.origin, 'option')
        else:
            with self.assertRaises(InvalidOptionError) as e:
                option.get_value('--%s-option' % enable, 'option')
            if nargs == 1:
                self.assertEquals(e.exception.message,
                                  '--%s-option takes 1 value' % enable)
            elif nargs == '+':
                self.assertEquals(e.exception.message,
                                  '--%s-option takes 1 or more values'
                                  % enable)
            else:
                self.assertEquals(e.exception.message,
                                  '--%s-option takes 2 values' % enable)

    def test_option_value_with(self):
        self.test_option_value_enable('with', 'without')

    def test_option_value_invalid_nargs(self):
        with self.assertRaises(InvalidOptionError) as e:
            Option('--option', nargs='foo')
        self.assertEquals(e.exception.message,
                          "nargs must be a positive integer, '?', '*' or '+'")

        with self.assertRaises(InvalidOptionError) as e:
            Option('--option', nargs=-2)
        self.assertEquals(e.exception.message,
                          "nargs must be a positive integer, '?', '*' or '+'")

    def test_option_value_nargs_1(self):
        self.test_option_value(nargs=1)
        self.test_option_value(nargs=1, default=('a',))
        self.test_option_value_enable(nargs=1, default=('a',))

        # A default is required
        with self.assertRaises(InvalidOptionError) as e:
            Option('--disable-option', nargs=1)
        self.assertEquals(e.exception.message,
                          "The given `default` doesn't satisfy `nargs`")

    def test_option_value_nargs_2(self):
        self.test_option_value(nargs=2)
        self.test_option_value(nargs=2, default=('a', 'b'))
        self.test_option_value_enable(nargs=2, default=('a', 'b'))

        # A default is required
        with self.assertRaises(InvalidOptionError) as e:
            Option('--disable-option', nargs=2)
        self.assertEquals(e.exception.message,
                          "The given `default` doesn't satisfy `nargs`")

    def test_option_value_nargs_0_or_1(self):
        self.test_option_value(nargs='?')
        self.test_option_value(nargs='?', default=('a',))
        self.test_option_value_enable(nargs='?')
        self.test_option_value_enable(nargs='?', default=('a',))

    def test_option_value_nargs_0_or_more(self):
        self.test_option_value(nargs='*')
        self.test_option_value(nargs='*', default=('a',))
        self.test_option_value(nargs='*', default=('a', 'b'))
        self.test_option_value_enable(nargs='*')
        self.test_option_value_enable(nargs='*', default=('a',))
        self.test_option_value_enable(nargs='*', default=('a', 'b'))

    def test_option_value_nargs_1_or_more(self):
        self.test_option_value(nargs='+')
        self.test_option_value(nargs='+', default=('a',))
        self.test_option_value(nargs='+', default=('a', 'b'))
        self.test_option_value_enable(nargs='+', default=('a',))
        self.test_option_value_enable(nargs='+', default=('a', 'b'))

        # A default is required
        with self.assertRaises(InvalidOptionError) as e:
            Option('--disable-option', nargs='+')
        self.assertEquals(e.exception.message,
                          "The given `default` doesn't satisfy `nargs`")


class TestCommandLineHelper(unittest.TestCase):
    def test_basic(self):
        helper = CommandLineHelper({}, ['cmd', '--foo', '--bar'])

        self.assertEquals(['--foo', '--bar'], list(helper))

        helper.add('--enable-qux')

        self.assertEquals(['--foo', '--bar', '--enable-qux'], list(helper))

        value, option = helper.handle(Option('--bar'))
        self.assertEquals(['--foo', '--enable-qux'], list(helper))
        self.assertEquals(PositiveOptionValue(), value)
        self.assertEquals('--bar', option)

        value, option = helper.handle(Option('--baz'))
        self.assertEquals(['--foo', '--enable-qux'], list(helper))
        self.assertEquals(NegativeOptionValue(), value)
        self.assertEquals(None, option)

        with self.assertRaises(AssertionError):
            CommandLineHelper({}, ['--foo', '--bar'])

    def test_precedence(self):
        foo = Option('--with-foo', nargs='*')
        helper = CommandLineHelper({}, ['cmd', '--with-foo=a,b'])
        value, option = helper.handle(foo)
        self.assertEquals(PositiveOptionValue(('a', 'b')), value)
        self.assertEquals('command-line', value.origin)
        self.assertEquals('--with-foo=a,b', option)

        helper = CommandLineHelper({}, ['cmd', '--with-foo=a,b',
                                        '--without-foo'])
        value, option = helper.handle(foo)
        self.assertEquals(NegativeOptionValue(), value)
        self.assertEquals('command-line', value.origin)
        self.assertEquals('--without-foo', option)

        helper = CommandLineHelper({}, ['cmd', '--without-foo',
                                        '--with-foo=a,b'])
        value, option = helper.handle(foo)
        self.assertEquals(PositiveOptionValue(('a', 'b')), value)
        self.assertEquals('command-line', value.origin)
        self.assertEquals('--with-foo=a,b', option)

        foo = Option('--with-foo', env='FOO', nargs='*')
        helper = CommandLineHelper({'FOO': ''}, ['cmd', '--with-foo=a,b'])
        value, option = helper.handle(foo)
        self.assertEquals(PositiveOptionValue(('a', 'b')), value)
        self.assertEquals('command-line', value.origin)
        self.assertEquals('--with-foo=a,b', option)

        helper = CommandLineHelper({'FOO': 'a,b'}, ['cmd', '--without-foo'])
        value, option = helper.handle(foo)
        self.assertEquals(NegativeOptionValue(), value)
        self.assertEquals('command-line', value.origin)
        self.assertEquals('--without-foo', option)

        helper = CommandLineHelper({'FOO': ''}, ['cmd', '--with-bar=a,b'])
        value, option = helper.handle(foo)
        self.assertEquals(NegativeOptionValue(), value)
        self.assertEquals('environment', value.origin)
        self.assertEquals('FOO=', option)

        helper = CommandLineHelper({'FOO': 'a,b'}, ['cmd', '--without-bar'])
        value, option = helper.handle(foo)
        self.assertEquals(PositiveOptionValue(('a', 'b')), value)
        self.assertEquals('environment', value.origin)
        self.assertEquals('FOO=a,b', option)

        helper = CommandLineHelper({}, ['cmd', '--with-foo=a,b', 'FOO='])
        value, option = helper.handle(foo)
        self.assertEquals(NegativeOptionValue(), value)
        self.assertEquals('command-line', value.origin)
        self.assertEquals('FOO=', option)

        helper = CommandLineHelper({}, ['cmd', '--without-foo', 'FOO=a,b'])
        value, option = helper.handle(foo)
        self.assertEquals(PositiveOptionValue(('a', 'b')), value)
        self.assertEquals('command-line', value.origin)
        self.assertEquals('FOO=a,b', option)

        helper = CommandLineHelper({}, ['cmd', 'FOO=', '--with-foo=a,b'])
        value, option = helper.handle(foo)
        self.assertEquals(PositiveOptionValue(('a', 'b')), value)
        self.assertEquals('command-line', value.origin)
        self.assertEquals('--with-foo=a,b', option)

        helper = CommandLineHelper({}, ['cmd', 'FOO=a,b', '--without-foo'])
        value, option = helper.handle(foo)
        self.assertEquals(NegativeOptionValue(), value)
        self.assertEquals('command-line', value.origin)
        self.assertEquals('--without-foo', option)

    def test_extra_args(self):
        foo = Option('--with-foo', env='FOO', nargs='*')
        helper = CommandLineHelper({}, ['cmd'])
        helper.add('FOO=a,b,c', 'other-origin')
        value, option = helper.handle(foo)
        self.assertEquals(PositiveOptionValue(('a', 'b', 'c')), value)
        self.assertEquals('other-origin', value.origin)
        self.assertEquals('FOO=a,b,c', option)

        helper = CommandLineHelper({}, ['cmd'])
        helper.add('FOO=a,b,c', 'other-origin')
        helper.add('--with-foo=a,b,c', 'other-origin')
        value, option = helper.handle(foo)
        self.assertEquals(PositiveOptionValue(('a', 'b', 'c')), value)
        self.assertEquals('other-origin', value.origin)
        self.assertEquals('--with-foo=a,b,c', option)

        # Adding conflicting options is not allowed.
        helper = CommandLineHelper({}, ['cmd'])
        helper.add('FOO=a,b,c', 'other-origin')
        with self.assertRaises(ConflictingOptionError) as cm:
            helper.add('FOO=', 'other-origin')
        self.assertEqual('FOO=', cm.exception.arg)
        self.assertEqual('other-origin', cm.exception.origin)
        self.assertEqual('FOO=a,b,c', cm.exception.old_arg)
        self.assertEqual('other-origin', cm.exception.old_origin)
        with self.assertRaises(ConflictingOptionError) as cm:
            helper.add('FOO=a,b', 'other-origin')
        self.assertEqual('FOO=a,b', cm.exception.arg)
        self.assertEqual('other-origin', cm.exception.origin)
        self.assertEqual('FOO=a,b,c', cm.exception.old_arg)
        self.assertEqual('other-origin', cm.exception.old_origin)
        # But adding the same is allowed.
        helper.add('FOO=a,b,c', 'other-origin')
        value, option = helper.handle(foo)
        self.assertEquals(PositiveOptionValue(('a', 'b', 'c')), value)
        self.assertEquals('other-origin', value.origin)
        self.assertEquals('FOO=a,b,c', option)

        # The same rule as above applies when using the option form vs. the
        # variable form. But we can't detect it when .add is called.
        helper = CommandLineHelper({}, ['cmd'])
        helper.add('FOO=a,b,c', 'other-origin')
        helper.add('--without-foo', 'other-origin')
        with self.assertRaises(ConflictingOptionError) as cm:
            helper.handle(foo)
        self.assertEqual('--without-foo', cm.exception.arg)
        self.assertEqual('other-origin', cm.exception.origin)
        self.assertEqual('FOO=a,b,c', cm.exception.old_arg)
        self.assertEqual('other-origin', cm.exception.old_origin)
        helper = CommandLineHelper({}, ['cmd'])
        helper.add('FOO=a,b,c', 'other-origin')
        helper.add('--with-foo=a,b', 'other-origin')
        with self.assertRaises(ConflictingOptionError) as cm:
            helper.handle(foo)
        self.assertEqual('--with-foo=a,b', cm.exception.arg)
        self.assertEqual('other-origin', cm.exception.origin)
        self.assertEqual('FOO=a,b,c', cm.exception.old_arg)
        self.assertEqual('other-origin', cm.exception.old_origin)
        helper = CommandLineHelper({}, ['cmd'])
        helper.add('FOO=a,b,c', 'other-origin')
        helper.add('--with-foo=a,b,c', 'other-origin')
        value, option = helper.handle(foo)
        self.assertEquals(PositiveOptionValue(('a', 'b', 'c')), value)
        self.assertEquals('other-origin', value.origin)
        self.assertEquals('--with-foo=a,b,c', option)

        # Conflicts are also not allowed against what is in the
        # environment/on the command line.
        helper = CommandLineHelper({}, ['cmd', '--with-foo=a,b'])
        helper.add('FOO=a,b,c', 'other-origin')
        with self.assertRaises(ConflictingOptionError) as cm:
            helper.handle(foo)
        self.assertEqual('FOO=a,b,c', cm.exception.arg)
        self.assertEqual('other-origin', cm.exception.origin)
        self.assertEqual('--with-foo=a,b', cm.exception.old_arg)
        self.assertEqual('command-line', cm.exception.old_origin)

        helper = CommandLineHelper({}, ['cmd', '--with-foo=a,b'])
        helper.add('--without-foo', 'other-origin')
        with self.assertRaises(ConflictingOptionError) as cm:
            helper.handle(foo)
        self.assertEqual('--without-foo', cm.exception.arg)
        self.assertEqual('other-origin', cm.exception.origin)
        self.assertEqual('--with-foo=a,b', cm.exception.old_arg)
        self.assertEqual('command-line', cm.exception.old_origin)

    def test_possible_origins(self):
        with self.assertRaises(InvalidOptionError):
            Option('--foo', possible_origins='command-line')

        helper = CommandLineHelper({'BAZ': '1'}, ['cmd', '--foo', '--bar'])
        foo = Option('--foo',
                     possible_origins=('command-line',))
        value, option = helper.handle(foo)
        self.assertEquals(PositiveOptionValue(), value)
        self.assertEquals('command-line', value.origin)
        self.assertEquals('--foo', option)

        bar = Option('--bar',
                     possible_origins=('mozconfig',))
        with self.assertRaisesRegexp(InvalidOptionError,
            "--bar can not be set by command-line. Values are accepted from: mozconfig"):
            helper.handle(bar)

        baz = Option(env='BAZ',
                     possible_origins=('implied',))
        with self.assertRaisesRegexp(InvalidOptionError,
            "BAZ=1 can not be set by environment. Values are accepted from: implied"):
            helper.handle(baz)


if __name__ == '__main__':
    main()
