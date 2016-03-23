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
from mozbuild.configure import (
    ConfigureError,
    ConfigureSandbox,
)

import mozpack.path as mozpath

test_data_path = mozpath.abspath(mozpath.dirname(__file__))
test_data_path = mozpath.join(test_data_path, 'data')


class TestConfigure(unittest.TestCase):
    def get_result(self, args=[], environ={}, configure='moz.configure',
                   prog='/bin/configure'):
        config = {}
        out = StringIO()
        sandbox = ConfigureSandbox(config, environ, [prog] + args, out, out)

        sandbox.run(mozpath.join(test_data_path, configure))

        return config, out.getvalue()

    def get_config(self, options=[], env={}, **kwargs):
        config, out = self.get_result(options, environ=env, **kwargs)
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
            '  --returned-choices        Choices\n'
            '  --enable-advanced-template\n'
            '                            Advanced template\n'
            '  --enable-include          Include\n'
            '  --with-advanced           Advanced\n'
            '\n'
            'Environment variables:\n'
            '  CC                        C Compiler\n',
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

    def test_set_config(self):
        def get_config(*args):
            return self.get_config(*args, configure='set_config.configure')

        config, out = self.get_result(['--help'],
                                      configure='set_config.configure')
        self.assertEquals(config, {})

        config = get_config(['--set-foo'])
        self.assertIn('FOO', config)
        self.assertEquals(config['FOO'], True)

        config = get_config(['--set-bar'])
        self.assertNotIn('FOO', config)
        self.assertIn('BAR', config)
        self.assertEquals(config['BAR'], True)

        config = get_config(['--set-value=qux'])
        self.assertIn('VALUE', config)
        self.assertEquals(config['VALUE'], 'qux')

        config = get_config(['--set-name=hoge'])
        self.assertIn('hoge', config)
        self.assertEquals(config['hoge'], True)

        config = get_config([])
        self.assertEquals(config, {'BAR': False})

        with self.assertRaises(ConfigureError):
            # Both --set-foo and --set-name=FOO are going to try to
            # set_config('FOO'...)
            get_config(['--set-foo', '--set-name=FOO'])

    def test_set_define(self):
        def get_config(*args):
            return self.get_config(*args, configure='set_define.configure')

        config, out = self.get_result(['--help'],
                                      configure='set_define.configure')
        self.assertEquals(config, {'DEFINES': {}})

        config = get_config(['--set-foo'])
        self.assertIn('FOO', config['DEFINES'])
        self.assertEquals(config['DEFINES']['FOO'], True)

        config = get_config(['--set-bar'])
        self.assertNotIn('FOO', config['DEFINES'])
        self.assertIn('BAR', config['DEFINES'])
        self.assertEquals(config['DEFINES']['BAR'], True)

        config = get_config(['--set-value=qux'])
        self.assertIn('VALUE', config['DEFINES'])
        self.assertEquals(config['DEFINES']['VALUE'], 'qux')

        config = get_config(['--set-name=hoge'])
        self.assertIn('hoge', config['DEFINES'])
        self.assertEquals(config['DEFINES']['hoge'], True)

        config = get_config([])
        self.assertEquals(config['DEFINES'], {'BAR': False})

        with self.assertRaises(ConfigureError):
            # Both --set-foo and --set-name=FOO are going to try to
            # set_define('FOO'...)
            get_config(['--set-foo', '--set-name=FOO'])

    def test_imply_option_simple(self):
        config = self.get_config([], configure='imply_option/simple.configure')
        self.assertEquals(config, {})

        config = self.get_config(['--enable-foo'],
                                 configure='imply_option/simple.configure')
        self.assertIn('BAR', config)
        self.assertEquals(config['BAR'], PositiveOptionValue())

        with self.assertRaises(InvalidOptionError) as e:
            config = self.get_config(['--enable-foo', '--disable-bar'],
                                     configure='imply_option/simple.configure')

        self.assertEquals(
            e.exception.message,
            "'--enable-bar' implied by '--enable-foo' conflicts with "
            "'--disable-bar' from the command-line")

    def test_imply_option_negative(self):
        config = self.get_config([],
                                 configure='imply_option/negative.configure')
        self.assertEquals(config, {})

        config = self.get_config(['--enable-foo'],
                                 configure='imply_option/negative.configure')
        self.assertIn('BAR', config)
        self.assertEquals(config['BAR'], NegativeOptionValue())

        with self.assertRaises(InvalidOptionError) as e:
            config = self.get_config(
                ['--enable-foo', '--enable-bar'],
                configure='imply_option/negative.configure')

        self.assertEquals(
            e.exception.message,
            "'--disable-bar' implied by '--enable-foo' conflicts with "
            "'--enable-bar' from the command-line")

        config = self.get_config(['--disable-hoge'],
                                 configure='imply_option/negative.configure')
        self.assertIn('BAR', config)
        self.assertEquals(config['BAR'], NegativeOptionValue())

        with self.assertRaises(InvalidOptionError) as e:
            config = self.get_config(
                ['--disable-hoge', '--enable-bar'],
                configure='imply_option/negative.configure')

        self.assertEquals(
            e.exception.message,
            "'--disable-bar' implied by '--disable-hoge' conflicts with "
            "'--enable-bar' from the command-line")

    def test_imply_option_values(self):
        config = self.get_config([], configure='imply_option/values.configure')
        self.assertEquals(config, {})

        config = self.get_config(['--enable-foo=a'],
                                 configure='imply_option/values.configure')
        self.assertIn('BAR', config)
        self.assertEquals(config['BAR'], PositiveOptionValue(('a',)))

        config = self.get_config(['--enable-foo=a,b'],
                                 configure='imply_option/values.configure')
        self.assertIn('BAR', config)
        self.assertEquals(config['BAR'], PositiveOptionValue(('a','b')))

        with self.assertRaises(InvalidOptionError) as e:
            config = self.get_config(['--enable-foo=a,b', '--disable-bar'],
                                     configure='imply_option/values.configure')

        self.assertEquals(
            e.exception.message,
            "'--enable-bar=a,b' implied by '--enable-foo' conflicts with "
            "'--disable-bar' from the command-line")

    def test_imply_option_infer(self):
        config = self.get_config([], configure='imply_option/infer.configure')

        with self.assertRaises(InvalidOptionError) as e:
            config = self.get_config(['--enable-foo', '--disable-bar'],
                                     configure='imply_option/infer.configure')

        self.assertEquals(
            e.exception.message,
            "'--enable-bar' implied by '--enable-foo' conflicts with "
            "'--disable-bar' from the command-line")

        with self.assertRaises(ConfigureError) as e:
            self.get_config([], configure='imply_option/infer_ko.configure')

        self.assertEquals(
            e.exception.message,
            "Cannot infer what implies '--enable-bar'. Please add a `reason` "
            "to the `imply_option` call.")


if __name__ == '__main__':
    main()
