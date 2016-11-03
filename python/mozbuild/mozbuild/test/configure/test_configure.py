# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from StringIO import StringIO
import os
import sys
import textwrap
import unittest

from mozunit import (
    main,
    MockedOpen,
)

from mozbuild.configure.options import (
    InvalidOptionError,
    NegativeOptionValue,
    PositiveOptionValue,
)
from mozbuild.configure import (
    ConfigureError,
    ConfigureSandbox,
)
from mozbuild.util import exec_

import mozpack.path as mozpath

test_data_path = mozpath.abspath(mozpath.dirname(__file__))
test_data_path = mozpath.join(test_data_path, 'data')


class TestConfigure(unittest.TestCase):
    def get_config(self, options=[], env={}, configure='moz.configure',
                   prog='/bin/configure'):
        config = {}
        out = StringIO()
        sandbox = ConfigureSandbox(config, env, [prog] + options, out, out)

        sandbox.run(mozpath.join(test_data_path, configure))

        if '--help' in options:
            return out.getvalue(), config
        self.assertEquals('', out.getvalue())
        return config

    def moz_configure(self, source):
        return MockedOpen({
            os.path.join(test_data_path,
                         'moz.configure'): textwrap.dedent(source)
        })

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
        help, config = self.get_config(['--help'], prog='configure')

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
            '  --enable-imports-in-template\n'
            '                            Imports in template\n'
            '  --enable-include          Include\n'
            '  --with-imports            Imports\n'
            '\n'
            'Environment variables:\n'
            '  CC                        C Compiler\n',
            help
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

    def test_imports(self):
        config = {}
        out = StringIO()
        sandbox = ConfigureSandbox(config, {}, [], out, out)

        with self.assertRaises(ImportError):
            exec_(textwrap.dedent('''
                @template
                def foo():
                    import sys
                foo()'''),
                sandbox
            )

        exec_(textwrap.dedent('''
            @template
            @imports('sys')
            def foo():
                return sys'''),
            sandbox
        )

        self.assertIs(sandbox['foo'](), sys)

        exec_(textwrap.dedent('''
            @template
            @imports(_from='os', _import='path')
            def foo():
                return path'''),
            sandbox
        )

        self.assertIs(sandbox['foo'](), os.path)

        exec_(textwrap.dedent('''
            @template
            @imports(_from='os', _import='path', _as='os_path')
            def foo():
                return os_path'''),
            sandbox
        )

        self.assertIs(sandbox['foo'](), os.path)

        exec_(textwrap.dedent('''
            @template
            @imports('__builtin__')
            def foo():
                return __builtin__'''),
            sandbox
        )

        import __builtin__
        self.assertIs(sandbox['foo'](), __builtin__)

        exec_(textwrap.dedent('''
            @template
            @imports(_from='__builtin__', _import='open')
            def foo():
                return open('%s')''' % os.devnull),
            sandbox
        )

        f = sandbox['foo']()
        self.assertEquals(f.name, os.devnull)
        f.close()

        # This unlocks the sandbox
        exec_(textwrap.dedent('''
            @template
            @imports(_import='__builtin__', _as='__builtins__')
            def foo():
                import sys
                return sys'''),
            sandbox
        )

        self.assertIs(sandbox['foo'](), sys)

        exec_(textwrap.dedent('''
            @template
            @imports('__sandbox__')
            def foo():
                return __sandbox__'''),
            sandbox
        )

        self.assertIs(sandbox['foo'](), sandbox)

        exec_(textwrap.dedent('''
            @template
            @imports(_import='__sandbox__', _as='s')
            def foo():
                return s'''),
            sandbox
        )

        self.assertIs(sandbox['foo'](), sandbox)

        # Nothing leaked from the function being executed
        self.assertEquals(sandbox.keys(), ['__builtins__', 'foo'])
        self.assertEquals(sandbox['__builtins__'], ConfigureSandbox.BUILTINS)

    def test_apply_imports(self):
        imports = []

        class CountApplyImportsSandbox(ConfigureSandbox):
            def _apply_imports(self, *args, **kwargs):
                imports.append((args, kwargs))
                super(CountApplyImportsSandbox, self)._apply_imports(
                    *args, **kwargs)

        config = {}
        out = StringIO()
        sandbox = CountApplyImportsSandbox(config, {}, [], out, out)

        exec_(textwrap.dedent('''
            @template
            @imports('sys')
            def foo():
                return sys
            foo()
            foo()'''),
            sandbox
        )

        self.assertEquals(len(imports), 1)

    def test_os_path(self):
        config = self.get_config(['--with-imports=%s' % __file__])
        self.assertIn('HAS_ABSPATH', config)
        self.assertEquals(config['HAS_ABSPATH'], True)
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

    def test_template_imports(self):
        config = self.get_config(['--enable-imports-in-template'])
        self.assertIn('PLATFORM', config)
        self.assertEquals(config['PLATFORM'], sys.platform)

    def test_decorators(self):
        config = {}
        out = StringIO()
        sandbox = ConfigureSandbox(config, {}, [], out, out)

        sandbox.include_file(mozpath.join(test_data_path, 'decorators.configure'))

        self.assertNotIn('FOO', sandbox)
        self.assertNotIn('BAR', sandbox)
        self.assertNotIn('QUX', sandbox)

    def test_set_config(self):
        def get_config(*args):
            return self.get_config(*args, configure='set_config.configure')

        help, config = get_config(['--help'])
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

    def test_set_config_when(self):
        with self.moz_configure('''
            option('--with-qux', help='qux')
            set_config('FOO', 'foo', when=True)
            set_config('BAR', 'bar', when=False)
            set_config('QUX', 'qux', when='--with-qux')
        '''):
            config = self.get_config()
            self.assertEquals(config, {
                'FOO': 'foo',
            })
            config = self.get_config(['--with-qux'])
            self.assertEquals(config, {
                'FOO': 'foo',
                'QUX': 'qux',
            })

    def test_set_define(self):
        def get_config(*args):
            return self.get_config(*args, configure='set_define.configure')

        help, config = get_config(['--help'])
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

    def test_set_define_when(self):
        with self.moz_configure('''
            option('--with-qux', help='qux')
            set_define('FOO', 'foo', when=True)
            set_define('BAR', 'bar', when=False)
            set_define('QUX', 'qux', when='--with-qux')
        '''):
            config = self.get_config()
            self.assertEquals(config['DEFINES'], {
                'FOO': 'foo',
            })
            config = self.get_config(['--with-qux'])
            self.assertEquals(config['DEFINES'], {
                'FOO': 'foo',
                'QUX': 'qux',
            })

    def test_imply_option_simple(self):
        def get_config(*args):
            return self.get_config(
                *args, configure='imply_option/simple.configure')

        help, config = get_config(['--help'])
        self.assertEquals(config, {})

        config = get_config([])
        self.assertEquals(config, {})

        config = get_config(['--enable-foo'])
        self.assertIn('BAR', config)
        self.assertEquals(config['BAR'], PositiveOptionValue())

        with self.assertRaises(InvalidOptionError) as e:
            get_config(['--enable-foo', '--disable-bar'])

        self.assertEquals(
            e.exception.message,
            "'--enable-bar' implied by '--enable-foo' conflicts with "
            "'--disable-bar' from the command-line")

    def test_imply_option_negative(self):
        def get_config(*args):
            return self.get_config(
                *args, configure='imply_option/negative.configure')

        help, config = get_config(['--help'])
        self.assertEquals(config, {})

        config = get_config([])
        self.assertEquals(config, {})

        config = get_config(['--enable-foo'])
        self.assertIn('BAR', config)
        self.assertEquals(config['BAR'], NegativeOptionValue())

        with self.assertRaises(InvalidOptionError) as e:
            get_config(['--enable-foo', '--enable-bar'])

        self.assertEquals(
            e.exception.message,
            "'--disable-bar' implied by '--enable-foo' conflicts with "
            "'--enable-bar' from the command-line")

        config = get_config(['--disable-hoge'])
        self.assertIn('BAR', config)
        self.assertEquals(config['BAR'], NegativeOptionValue())

        with self.assertRaises(InvalidOptionError) as e:
            get_config(['--disable-hoge', '--enable-bar'])

        self.assertEquals(
            e.exception.message,
            "'--disable-bar' implied by '--disable-hoge' conflicts with "
            "'--enable-bar' from the command-line")

    def test_imply_option_values(self):
        def get_config(*args):
            return self.get_config(
                *args, configure='imply_option/values.configure')

        help, config = get_config(['--help'])
        self.assertEquals(config, {})

        config = get_config([])
        self.assertEquals(config, {})

        config = get_config(['--enable-foo=a'])
        self.assertIn('BAR', config)
        self.assertEquals(config['BAR'], PositiveOptionValue(('a',)))

        config = get_config(['--enable-foo=a,b'])
        self.assertIn('BAR', config)
        self.assertEquals(config['BAR'], PositiveOptionValue(('a','b')))

        with self.assertRaises(InvalidOptionError) as e:
            get_config(['--enable-foo=a,b', '--disable-bar'])

        self.assertEquals(
            e.exception.message,
            "'--enable-bar=a,b' implied by '--enable-foo' conflicts with "
            "'--disable-bar' from the command-line")

    def test_imply_option_infer(self):
        def get_config(*args):
            return self.get_config(
                *args, configure='imply_option/infer.configure')

        help, config = get_config(['--help'])
        self.assertEquals(config, {})

        config = get_config([])
        self.assertEquals(config, {})

        with self.assertRaises(InvalidOptionError) as e:
            get_config(['--enable-foo', '--disable-bar'])

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

    def test_imply_option_immediate_value(self):
        def get_config(*args):
            return self.get_config(
                *args, configure='imply_option/imm.configure')

        help, config = get_config(['--help'])
        self.assertEquals(config, {})

        config = get_config([])
        self.assertEquals(config, {})

        config_path = mozpath.abspath(
            mozpath.join(test_data_path, 'imply_option', 'imm.configure'))

        with self.assertRaisesRegexp(InvalidOptionError,
            "--enable-foo' implied by 'imply_option at %s:7' conflicts with "
            "'--disable-foo' from the command-line" % config_path):
            get_config(['--disable-foo'])

        with self.assertRaisesRegexp(InvalidOptionError,
            "--enable-bar=foo,bar' implied by 'imply_option at %s:16' conflicts"
            " with '--enable-bar=a,b,c' from the command-line" % config_path):
            get_config(['--enable-bar=a,b,c'])

        with self.assertRaisesRegexp(InvalidOptionError,
            "--enable-baz=BAZ' implied by 'imply_option at %s:25' conflicts"
            " with '--enable-baz=QUUX' from the command-line" % config_path):
            get_config(['--enable-baz=QUUX'])

    def test_imply_option_failures(self):
        with self.assertRaises(ConfigureError) as e:
            with self.moz_configure('''
                imply_option('--with-foo', ('a',), 'bar')
            '''):
                self.get_config()

        self.assertEquals(e.exception.message,
                          "`--with-foo`, emitted from `%s` line 2, is unknown."
                          % mozpath.join(test_data_path, 'moz.configure'))

        with self.assertRaises(TypeError) as e:
            with self.moz_configure('''
                imply_option('--with-foo', 42, 'bar')

                option('--with-foo', help='foo')
                @depends('--with-foo')
                def foo(value):
                    return value
            '''):
                self.get_config()

        self.assertEquals(e.exception.message,
                          "Unexpected type: 'int'")

    def test_imply_option_when(self):
        with self.moz_configure('''
            option('--with-foo', help='foo')
            imply_option('--with-qux', True, when='--with-foo')
            option('--with-qux', help='qux')
            set_config('QUX', depends('--with-qux')(lambda x: x))
        '''):
            config = self.get_config()
            self.assertEquals(config, {
                'QUX': NegativeOptionValue(),
            })

            config = self.get_config(['--with-foo'])
            self.assertEquals(config, {
                'QUX': PositiveOptionValue(),
            })

    def test_option_failures(self):
        with self.assertRaises(ConfigureError) as e:
            with self.moz_configure('option("--with-foo", help="foo")'):
                self.get_config()

        self.assertEquals(
            e.exception.message,
            'Option `--with-foo` is not handled ; reference it with a @depends'
        )

        with self.assertRaises(ConfigureError) as e:
            with self.moz_configure('''
                option("--with-foo", help="foo")
                option("--with-foo", help="foo")
            '''):
                self.get_config()

        self.assertEquals(
            e.exception.message,
            'Option `--with-foo` already defined'
        )

        with self.assertRaises(ConfigureError) as e:
            with self.moz_configure('''
                option(env="MOZ_FOO", help="foo")
                option(env="MOZ_FOO", help="foo")
            '''):
                self.get_config()

        self.assertEquals(
            e.exception.message,
            'Option `MOZ_FOO` already defined'
        )

        with self.assertRaises(ConfigureError) as e:
            with self.moz_configure('''
                option('--with-foo', env="MOZ_FOO", help="foo")
                option(env="MOZ_FOO", help="foo")
            '''):
                self.get_config()

        self.assertEquals(
            e.exception.message,
            'Option `MOZ_FOO` already defined'
        )

        with self.assertRaises(ConfigureError) as e:
            with self.moz_configure('''
                option(env="MOZ_FOO", help="foo")
                option('--with-foo', env="MOZ_FOO", help="foo")
            '''):
                self.get_config()

        self.assertEquals(
            e.exception.message,
            'Option `MOZ_FOO` already defined'
        )

        with self.assertRaises(ConfigureError) as e:
            with self.moz_configure('''
                option('--with-foo', env="MOZ_FOO", help="foo")
                option('--with-foo', help="foo")
            '''):
                self.get_config()

        self.assertEquals(
            e.exception.message,
            'Option `--with-foo` already defined'
        )

    def test_option_when(self):
        with self.moz_configure('''
            option('--with-foo', help='foo', when=True)
            option('--with-bar', help='bar', when=False)
            option('--with-qux', env="QUX", help='qux', when='--with-foo')

            set_config('FOO', depends('--with-foo', when=True)(lambda x: x))
            set_config('BAR', depends('--with-bar', when=False)(lambda x: x))
            set_config('QUX', depends('--with-qux', when='--with-foo')(lambda x: x))
        '''):
            config = self.get_config()
            self.assertEquals(config, {
                'FOO': NegativeOptionValue(),
            })

            config = self.get_config(['--with-foo'])
            self.assertEquals(config, {
                'FOO': PositiveOptionValue(),
                'QUX': NegativeOptionValue(),
            })

            config = self.get_config(['--with-foo', '--with-qux'])
            self.assertEquals(config, {
                'FOO': PositiveOptionValue(),
                'QUX': PositiveOptionValue(),
            })

            with self.assertRaises(InvalidOptionError) as e:
                self.get_config(['--with-bar'])

            self.assertEquals(
                e.exception.message,
                '--with-bar is not available in this configuration'
            )

            with self.assertRaises(InvalidOptionError) as e:
                self.get_config(['--with-qux'])

            self.assertEquals(
                e.exception.message,
                '--with-qux is not available in this configuration'
            )

            with self.assertRaises(InvalidOptionError) as e:
                self.get_config(['QUX=1'])

            self.assertEquals(
                e.exception.message,
                'QUX is not available in this configuration'
            )

            config = self.get_config(env={'QUX': '1'})
            self.assertEquals(config, {
                'FOO': NegativeOptionValue(),
            })

            help, config = self.get_config(['--help'])
            self.assertEquals(help, textwrap.dedent('''\
                Usage: configure [options]

                Options: [defaults in brackets after descriptions]
                  --help                    print this message
                  --with-foo                foo

                Environment variables:
            '''))

            help, config = self.get_config(['--help', '--with-foo'])
            self.assertEquals(help, textwrap.dedent('''\
                Usage: configure [options]

                Options: [defaults in brackets after descriptions]
                  --help                    print this message
                  --with-foo                foo
                  --with-qux                qux

                Environment variables:
            '''))

        with self.moz_configure('''
            option('--with-foo', help='foo', when=True)
            set_config('FOO', depends('--with-foo')(lambda x: x))
        '''):
            with self.assertRaises(ConfigureError) as e:
                self.get_config()

            self.assertEquals(e.exception.message,
                              '@depends function needs the same `when` as '
                              'options it depends on')

        with self.moz_configure('''
            @depends(when=True)
            def always():
                return True
            @depends(when=True)
            def always2():
                return True
            option('--with-foo', help='foo', when=always)
            set_config('FOO', depends('--with-foo', when=always2)(lambda x: x))
        '''):
            with self.assertRaises(ConfigureError) as e:
                self.get_config()

            self.assertEquals(e.exception.message,
                              '@depends function needs the same `when` as '
                              'options it depends on')

    def test_include_failures(self):
        with self.assertRaises(ConfigureError) as e:
            with self.moz_configure('include("../foo.configure")'):
                self.get_config()

        self.assertEquals(
            e.exception.message,
            'Cannot include `%s` because it is not in a subdirectory of `%s`'
            % (mozpath.normpath(mozpath.join(test_data_path, '..',
                                             'foo.configure')),
               mozpath.normsep(test_data_path))
        )

        with self.assertRaises(ConfigureError) as e:
            with self.moz_configure('''
                include('extra.configure')
                include('extra.configure')
            '''):
                self.get_config()

        self.assertEquals(
            e.exception.message,
            'Cannot include `%s` because it was included already.'
            % mozpath.normpath(mozpath.join(test_data_path,
                                            'extra.configure'))
        )

        with self.assertRaises(TypeError) as e:
            with self.moz_configure('''
                include(42)
            '''):
                self.get_config()

        self.assertEquals(e.exception.message, "Unexpected type: 'int'")

    def test_include_when(self):
        with MockedOpen({
            os.path.join(test_data_path, 'moz.configure'): textwrap.dedent('''
                option('--with-foo', help='foo')

                include('always.configure', when=True)
                include('never.configure', when=False)
                include('foo.configure', when='--with-foo')

                set_config('FOO', foo)
                set_config('BAR', bar)
                set_config('QUX', qux)
            '''),
            os.path.join(test_data_path, 'always.configure'): textwrap.dedent('''
                option('--with-bar', help='bar')
                @depends('--with-bar')
                def bar(x):
                    if x:
                        return 'bar'
            '''),
            os.path.join(test_data_path, 'never.configure'): textwrap.dedent('''
                option('--with-qux', help='qux')
                @depends('--with-qux')
                def qux(x):
                    if x:
                        return 'qux'
            '''),
            os.path.join(test_data_path, 'foo.configure'): textwrap.dedent('''
                option('--with-foo-really', help='really foo')
                @depends('--with-foo-really')
                def foo(x):
                    if x:
                        return 'foo'

                include('foo2.configure', when='--with-foo-really')
            '''),
            os.path.join(test_data_path, 'foo2.configure'): textwrap.dedent('''
                set_config('FOO2', True)
            '''),
        }):
            config = self.get_config()
            self.assertEquals(config, {})

            config = self.get_config(['--with-foo'])
            self.assertEquals(config, {})

            config = self.get_config(['--with-bar'])
            self.assertEquals(config, {
                'BAR': 'bar',
            })

            with self.assertRaises(InvalidOptionError) as e:
                self.get_config(['--with-qux'])

            self.assertEquals(
                e.exception.message,
                '--with-qux is not available in this configuration'
            )

            config = self.get_config(['--with-foo', '--with-foo-really'])
            self.assertEquals(config, {
                'FOO': 'foo',
                'FOO2': True,
            })

    def test_sandbox_failures(self):
        with self.assertRaises(KeyError) as e:
            with self.moz_configure('''
                include = 42
            '''):
                self.get_config()

        self.assertEquals(e.exception.message, 'Cannot reassign builtins')

        with self.assertRaises(KeyError) as e:
            with self.moz_configure('''
                foo = 42
            '''):
                self.get_config()

        self.assertEquals(e.exception.message,
                          'Cannot assign `foo` because it is neither a '
                          '@depends nor a @template')

    def test_depends_failures(self):
        with self.assertRaises(ConfigureError) as e:
            with self.moz_configure('''
                @depends()
                def foo():
                    return
            '''):
                self.get_config()

        self.assertEquals(e.exception.message,
                          "@depends needs at least one argument")

        with self.assertRaises(ConfigureError) as e:
            with self.moz_configure('''
                @depends('--with-foo')
                def foo(value):
                    return value
            '''):
                self.get_config()

        self.assertEquals(e.exception.message,
                          "'--with-foo' is not a known option. Maybe it's "
                          "declared too late?")

        with self.assertRaises(ConfigureError) as e:
            with self.moz_configure('''
                @depends('--with-foo=42')
                def foo(value):
                    return value
            '''):
                self.get_config()

        self.assertEquals(e.exception.message,
                          "Option must not contain an '='")

        with self.assertRaises(TypeError) as e:
            with self.moz_configure('''
                @depends(42)
                def foo(value):
                    return value
            '''):
                self.get_config()

        self.assertEquals(e.exception.message,
                          "Cannot use object of type 'int' as argument "
                          "to @depends")

        with self.assertRaises(ConfigureError) as e:
            with self.moz_configure('''
                @depends('--help')
                def foo(value):
                    yield
            '''):
                self.get_config()

        self.assertEquals(e.exception.message,
                          "Cannot decorate generator functions with @depends")

        with self.assertRaises(TypeError) as e:
            with self.moz_configure('''
                depends('--help')(42)
            '''):
                self.get_config()

        self.assertEquals(e.exception.message,
                          "Unexpected type: 'int'")

        with self.assertRaises(ConfigureError) as e:
            with self.moz_configure('''
                option('--foo', help='foo')
                @depends('--foo')
                def foo(value):
                    return value

                foo()
            '''):
                self.get_config()

        self.assertEquals(e.exception.message,
                          "The `foo` function may not be called")

        with self.assertRaises(TypeError) as e:
            with self.moz_configure('''
                @depends('--help', foo=42)
                def foo(_):
                    return
            '''):
                self.get_config()

        self.assertEquals(e.exception.message,
                          "depends_impl() got an unexpected keyword argument 'foo'")

    def test_depends_when(self):
        with self.moz_configure('''
            @depends(when=True)
            def foo():
                return 'foo'

            set_config('FOO', foo)

            @depends(when=False)
            def bar():
                return 'bar'

            set_config('BAR', bar)

            option('--with-qux', help='qux')
            @depends(when='--with-qux')
            def qux():
                return 'qux'

            set_config('QUX', qux)
        '''):
            config = self.get_config()
            self.assertEquals(config, {
                'FOO': 'foo',
            })

            config = self.get_config(['--with-qux'])
            self.assertEquals(config, {
                'FOO': 'foo',
                'QUX': 'qux',
            })

    def test_imports_failures(self):
        with self.assertRaises(ConfigureError) as e:
            with self.moz_configure('''
                @imports('os')
                @template
                def foo(value):
                    return value
            '''):
                self.get_config()

        self.assertEquals(e.exception.message,
                          '@imports must appear after @template')

        with self.assertRaises(ConfigureError) as e:
            with self.moz_configure('''
                option('--foo', help='foo')
                @imports('os')
                @depends('--foo')
                def foo(value):
                    return value
            '''):
                self.get_config()

        self.assertEquals(e.exception.message,
                          '@imports must appear after @depends')

        for import_ in (
            "42",
            "_from=42, _import='os'",
            "_from='os', _import='path', _as=42",
        ):
            with self.assertRaises(TypeError) as e:
                with self.moz_configure('''
                    @imports(%s)
                    @template
                    def foo(value):
                        return value
                ''' % import_):
                    self.get_config()

            self.assertEquals(e.exception.message, "Unexpected type: 'int'")

        with self.assertRaises(TypeError) as e:
            with self.moz_configure('''
                @imports('os', 42)
                @template
                def foo(value):
                    return value
            '''):
                self.get_config()

        self.assertEquals(e.exception.message, "Unexpected type: 'int'")

        with self.assertRaises(ValueError) as e:
            with self.moz_configure('''
                @imports('os*')
                def foo(value):
                    return value
            '''):
                self.get_config()

        self.assertEquals(e.exception.message,
                          "Invalid argument to @imports: 'os*'")

    def test_only_when(self):
        moz_configure = '''
            option('--enable-when', help='when')
            @depends('--enable-when', '--help')
            def when(value, _):
                return bool(value)

            with only_when(when):
                option('--foo', nargs='*', help='foo')
                @depends('--foo')
                def foo(value):
                    return value

                set_config('FOO', foo)
                set_define('FOO', foo)

            # It is possible to depend on a function defined in a only_when
            # block. It then resolves to `None`.
            set_config('BAR', depends(foo)(lambda x: x))
            set_define('BAR', depends(foo)(lambda x: x))
        '''

        with self.moz_configure(moz_configure):
            config = self.get_config()
            self.assertEqual(config, {
                'DEFINES': {},
            })

            config = self.get_config(['--enable-when'])
            self.assertEqual(config, {
                'BAR': NegativeOptionValue(),
                'FOO': NegativeOptionValue(),
                'DEFINES': {
                    'BAR': NegativeOptionValue(),
                    'FOO': NegativeOptionValue(),
                },
            })

            config = self.get_config(['--enable-when', '--foo=bar'])
            self.assertEqual(config, {
                'BAR': PositiveOptionValue(['bar']),
                'FOO': PositiveOptionValue(['bar']),
                'DEFINES': {
                    'BAR': PositiveOptionValue(['bar']),
                    'FOO': PositiveOptionValue(['bar']),
                },
            })

            # The --foo option doesn't exist when --enable-when is not given.
            with self.assertRaises(InvalidOptionError) as e:
                self.get_config(['--foo'])

            self.assertEquals(e.exception.message,
                              '--foo is not available in this configuration')

        # Cannot depend on an option defined in a only_when block, because we
        # don't know what OptionValue would make sense.
        with self.moz_configure(moz_configure + '''
            set_config('QUX', depends('--foo')(lambda x: x))
        '''):
            with self.assertRaises(ConfigureError) as e:
                self.get_config()

            self.assertEquals(e.exception.message,
                              '@depends function needs the same `when` as '
                              'options it depends on')

        with self.moz_configure(moz_configure + '''
            set_config('QUX', depends('--foo', when=when)(lambda x: x))
        '''):
            self.get_config(['--enable-when'])

        # Using imply_option for an option defined in a only_when block fails
        # similarly if the imply_option happens outside the block.
        with self.moz_configure('''
            imply_option('--foo', True)
        ''' + moz_configure):
            with self.assertRaises(InvalidOptionError) as e:
                self.get_config()

            self.assertEquals(e.exception.message,
                              '--foo is not available in this configuration')

        # And similarly doesn't fail when the condition is true.
        with self.moz_configure('''
            imply_option('--foo', True)
        ''' + moz_configure):
            self.get_config(['--enable-when'])


if __name__ == '__main__':
    main()
