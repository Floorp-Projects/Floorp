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

        if '--help' not in options:
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
        config = {}
        out = StringIO()
        sandbox = ConfigureSandbox(config, {}, ['configure', '--help'],
                                   out, out)
        sandbox.run(mozpath.join(test_data_path, 'moz.configure'))

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
            out.getvalue()
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
        self.assertIn('IS_FILE', config)
        self.assertEquals(config['IS_FILE'], True)

        config = self.get_config(['--with-imports=%s.no-exist' % __file__])
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

        config = get_config(['--help'])
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

        config = get_config(['--help'])
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
        def get_config(*args):
            return self.get_config(
                *args, configure='imply_option/simple.configure')

        config = get_config(['--help'])
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

        config = get_config(['--help'])
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

        config = get_config(['--help'])
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

        config = get_config(['--help'])
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

        config = get_config(['--help'])
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

        with self.assertRaises(ConfigureError) as e:
            with self.moz_configure('''
                option('--foo', help='foo')
                @depends('--foo')
                def foo(value):
                    return value

                @depends('--help', foo)
                def bar(help, foo):
                    return
            '''):
                self.get_config()

        self.assertEquals(e.exception.message,
                          "`bar` depends on '--help' and `foo`. "
                          "`foo` must depend on '--help'")

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

                include(foo)
            '''):
                self.get_config()

        self.assertEquals(e.exception.message,
                          "Missing @depends for `foo`: '--help'")

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


if __name__ == '__main__':
    main()
