# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from StringIO import StringIO
import sys
import unittest

from mozunit import main

from mozbuild.configure.options import (
    InvalidOptionError,
    NegativeOptionValue,
    PositiveOptionValue,
)
from mozbuild.configure import ConfigureSandbox

import mozpack.path as mozpath

test_data_path = mozpath.abspath(mozpath.dirname(__file__))
test_data_path = mozpath.join(test_data_path, 'data')


class TestConfigure(unittest.TestCase):
    def get_result(self, args=[], environ={}, prog='/bin/configure'):
        config = {}
        out = StringIO()
        sandbox = ConfigureSandbox(config, environ, [prog] + args, out, out)

        sandbox.run(mozpath.join(test_data_path, 'moz.configure'))

        return config, out.getvalue()

    def get_config(self, options=[], env={}):
        config, out = self.get_result(options, environ=env)
        self.assertEquals('', out)
        return config

    def test_defaults(self):
        config = self.get_config()
        self.maxDiff = None
        self.assertEquals({
            'CHOICES': NegativeOptionValue(),
            'DEFAULTED': PositiveOptionValue(('not-simple',)),
            'IS_GCC': NegativeOptionValue(),
            'REMAINDER': (PositiveOptionValue(), NegativeOptionValue(),
                          NegativeOptionValue(), NegativeOptionValue()),
            'SIMPLE': NegativeOptionValue(),
            'VALUES': NegativeOptionValue(),
            'VALUES2': NegativeOptionValue(),
            'VALUES3': NegativeOptionValue(),
            'WITH_ENV': NegativeOptionValue(),
            'IMPLIED': NegativeOptionValue(),
            'IMPLIED_ENV': NegativeOptionValue(),
            'IMPLIED_VALUES': NegativeOptionValue(),
        }, config)

    def test_help(self):
        config, out = self.get_result(['--help'])
        self.assertEquals({}, config)
        self.maxDiff = None
        self.assertEquals(
            'Usage: configure [options]\n'
            '\n'
            'Options: [defaults in brackets after descriptions]\n'
            '  --help                    print this message\n'
            '  --enable-simple           Enable simple\n'
            '  --enable-with-env         Enable with env\n'
            '  --enable-values           Enable values\n'
            '  --without-thing           Build without thing\n'
            '  --with-stuff              Build with stuff\n'
            '  --option                  Option\n'
            '  --with-returned-default   Returned default [not-simple]\n'
            '  --enable-implied          Implied\n'
            '  --with-implied-values     Implied values\n'
            '  --returned-choices        Choices\n'
            '  --enable-advanced-template\n'
            '                            Advanced template\n'
            '  --enable-include          Include\n'
            '  --with-advanced           Advanced\n'
            '\n'
            'Environment variables:\n'
            '  CC                        C Compiler\n'
            '  WITH_IMPLIED_ENV          Implied env\n',
            out
        )

    def test_unknown(self):
        with self.assertRaises(InvalidOptionError):
            self.get_config(['--unknown'])

    def test_simple(self):
        for config in (
                self.get_config(),
                self.get_config(['--disable-simple']),
                # Last option wins.
                self.get_config(['--enable-simple', '--disable-simple']),
        ):
            self.assertNotIn('ENABLED_SIMPLE', config)
            self.assertIn('SIMPLE', config)
            self.assertEquals(NegativeOptionValue(), config['SIMPLE'])

        for config in (
                self.get_config(['--enable-simple']),
                self.get_config(['--disable-simple', '--enable-simple']),
        ):
            self.assertIn('ENABLED_SIMPLE', config)
            self.assertIn('SIMPLE', config)
            self.assertEquals(PositiveOptionValue(), config['SIMPLE'])
            self.assertIs(config['SIMPLE'], config['ENABLED_SIMPLE'])

        # --enable-simple doesn't take values.
        with self.assertRaises(InvalidOptionError):
            self.get_config(['--enable-simple=value'])

    def test_with_env(self):
        for config in (
                self.get_config(),
                self.get_config(['--disable-with-env']),
                self.get_config(['--enable-with-env', '--disable-with-env']),
                self.get_config(env={'MOZ_WITH_ENV': ''}),
                # Options win over environment
                self.get_config(['--disable-with-env'],
                                env={'MOZ_WITH_ENV': '1'}),
        ):
            self.assertIn('WITH_ENV', config)
            self.assertEquals(NegativeOptionValue(), config['WITH_ENV'])

        for config in (
                self.get_config(['--enable-with-env']),
                self.get_config(['--disable-with-env', '--enable-with-env']),
                self.get_config(env={'MOZ_WITH_ENV': '1'}),
                self.get_config(['--enable-with-env'],
                                env={'MOZ_WITH_ENV': ''}),
        ):
            self.assertIn('WITH_ENV', config)
            self.assertEquals(PositiveOptionValue(), config['WITH_ENV'])

        with self.assertRaises(InvalidOptionError):
            self.get_config(['--enable-with-env=value'])

        with self.assertRaises(InvalidOptionError):
            self.get_config(env={'MOZ_WITH_ENV': 'value'})

    def test_values(self, name='VALUES'):
        for config in (
            self.get_config(),
            self.get_config(['--disable-values']),
            self.get_config(['--enable-values', '--disable-values']),
        ):
            self.assertIn(name, config)
            self.assertEquals(NegativeOptionValue(), config[name])

        for config in (
            self.get_config(['--enable-values']),
            self.get_config(['--disable-values', '--enable-values']),
        ):
            self.assertIn(name, config)
            self.assertEquals(PositiveOptionValue(), config[name])

        config = self.get_config(['--enable-values=foo'])
        self.assertIn(name, config)
        self.assertEquals(PositiveOptionValue(('foo',)), config[name])

        config = self.get_config(['--enable-values=foo,bar'])
        self.assertIn(name, config)
        self.assertTrue(config[name])
        self.assertEquals(PositiveOptionValue(('foo', 'bar')), config[name])

    def test_values2(self):
        self.test_values('VALUES2')

    def test_values3(self):
        self.test_values('VALUES3')

    def test_returned_default(self):
        config = self.get_config(['--enable-simple'])
        self.assertIn('DEFAULTED', config)
        self.assertEquals(
            PositiveOptionValue(('simple',)), config['DEFAULTED'])

        config = self.get_config(['--disable-simple'])
        self.assertIn('DEFAULTED', config)
        self.assertEquals(
            PositiveOptionValue(('not-simple',)), config['DEFAULTED'])

    def test_implied_options(self):
        config = self.get_config(['--enable-values'])
        self.assertIn('IMPLIED', config)
        self.assertIn('IMPLIED_VALUES', config)
        self.assertIn('IMPLIED_ENV', config)
        self.assertEquals(PositiveOptionValue(), config['IMPLIED'])
        self.assertEquals(PositiveOptionValue(), config['IMPLIED_VALUES'])
        self.assertEquals(PositiveOptionValue(), config['IMPLIED_ENV'])

        config = self.get_config(['--enable-values=a'])
        self.assertIn('IMPLIED', config)
        self.assertIn('IMPLIED_VALUES', config)
        self.assertIn('IMPLIED_ENV', config)
        self.assertEquals(PositiveOptionValue(), config['IMPLIED'])
        self.assertEquals(
            PositiveOptionValue(('a',)), config['IMPLIED_VALUES'])
        self.assertEquals(PositiveOptionValue(('a',)), config['IMPLIED_ENV'])

        config = self.get_config(['--enable-values=a,b'])
        self.assertIn('IMPLIED', config)
        self.assertIn('IMPLIED_VALUES', config)
        self.assertIn('IMPLIED_ENV', config)
        self.assertEquals(PositiveOptionValue(), config['IMPLIED'])
        self.assertEquals(
            PositiveOptionValue(('a', 'b')), config['IMPLIED_VALUES'])
        self.assertEquals(
            PositiveOptionValue(('a', 'b')), config['IMPLIED_ENV'])

        config = self.get_config(['--disable-values'])
        self.assertIn('IMPLIED', config)
        self.assertIn('IMPLIED_VALUES', config)
        self.assertIn('IMPLIED_ENV', config)
        self.assertEquals(NegativeOptionValue(), config['IMPLIED'])
        self.assertEquals(NegativeOptionValue(), config['IMPLIED_VALUES'])
        self.assertEquals(NegativeOptionValue(), config['IMPLIED_ENV'])

        # --enable-values implies --enable-implied, which conflicts with
        # --disable-implied
        with self.assertRaises(InvalidOptionError):
            self.get_config(['--enable-values', '--disable-implied'])

    def test_returned_choices(self):
        for val in ('a', 'b', 'c'):
            config = self.get_config(
                ['--enable-values=alpha', '--returned-choices=%s' % val])
            self.assertIn('CHOICES', config)
            self.assertEquals(PositiveOptionValue((val,)), config['CHOICES'])

        for val in ('0', '1', '2'):
            config = self.get_config(
                ['--enable-values=numeric', '--returned-choices=%s' % val])
            self.assertIn('CHOICES', config)
            self.assertEquals(PositiveOptionValue((val,)), config['CHOICES'])

        with self.assertRaises(InvalidOptionError):
            self.get_config(['--enable-values=numeric',
                             '--returned-choices=a'])

        with self.assertRaises(InvalidOptionError):
            self.get_config(['--enable-values=alpha', '--returned-choices=0'])

    def test_included(self):
        config = self.get_config(env={'CC': 'gcc'})
        self.assertIn('IS_GCC', config)
        self.assertEquals(config['IS_GCC'], True)

        config = self.get_config(
            ['--enable-include=extra.configure', '--extra'])
        self.assertIn('EXTRA', config)
        self.assertEquals(PositiveOptionValue(), config['EXTRA'])

        with self.assertRaises(InvalidOptionError):
            self.get_config(['--extra'])

    def test_template(self):
        config = self.get_config(env={'CC': 'gcc'})
        self.assertIn('CFLAGS', config)
        self.assertEquals(config['CFLAGS'], ['-Werror=foobar'])

        config = self.get_config(env={'CC': 'clang'})
        self.assertNotIn('CFLAGS', config)

    def test_advanced(self):
        config = self.get_config(['--with-advanced'])
        self.assertIn('ADVANCED', config)
        self.assertEquals(config['ADVANCED'], True)

        with self.assertRaises(ImportError):
            self.get_config(['--with-advanced=break'])

    def test_os_path(self):
        config = self.get_config(['--with-advanced=%s' % __file__])
        self.assertIn('IS_FILE', config)
        self.assertEquals(config['IS_FILE'], True)

        config = self.get_config(['--with-advanced=%s.no-exist' % __file__])
        self.assertIn('IS_FILE', config)
        self.assertEquals(config['IS_FILE'], False)

        self.assertIn('HAS_GETATIME', config)
        self.assertEquals(config['HAS_GETATIME'], True)
        self.assertIn('HAS_GETATIME2', config)
        self.assertEquals(config['HAS_GETATIME2'], False)

    def test_template_call(self):
        config = self.get_config(env={'CC': 'gcc'})
        self.assertIn('TEMPLATE_VALUE', config)
        self.assertEquals(config['TEMPLATE_VALUE'], 42)
        self.assertIn('TEMPLATE_VALUE_2', config)
        self.assertEquals(config['TEMPLATE_VALUE_2'], 21)

    def test_template_advanced(self):
        config = self.get_config(['--enable-advanced-template'])
        self.assertIn('PLATFORM', config)
        self.assertEquals(config['PLATFORM'], sys.platform)


if __name__ == '__main__':
    main()
