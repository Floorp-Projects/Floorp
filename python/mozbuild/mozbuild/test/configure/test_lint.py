# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import contextlib
import os
import sys
import textwrap
import traceback
import unittest

from mozunit import (
    main,
    MockedOpen,
)

from mozbuild.configure import ConfigureError
from mozbuild.configure.lint import LintSandbox

import mozpack.path as mozpath

test_data_path = mozpath.abspath(mozpath.dirname(__file__))
test_data_path = mozpath.join(test_data_path, 'data')


class TestLint(unittest.TestCase):
    def lint_test(self, options=[], env={}):
        sandbox = LintSandbox(env, ['configure'] + options)

        sandbox.run(mozpath.join(test_data_path, 'moz.configure'))

    def moz_configure(self, source):
        return MockedOpen({
            os.path.join(test_data_path,
                         'moz.configure'): textwrap.dedent(source)
        })

    @contextlib.contextmanager
    def assertRaisesFromLine(self, exc_type, line):
        with self.assertRaises(exc_type) as e:
            yield e

        _, _, tb = sys.exc_info()
        self.assertEquals(
            traceback.extract_tb(tb)[-1][:2],
            (mozpath.join(test_data_path, 'moz.configure'), line))

    def test_configure_testcase(self):
        # Lint python/mozbuild/mozbuild/test/configure/data/moz.configure
        self.lint_test()

    def test_depends_failures(self):
        with self.moz_configure('''
            option('--foo', help='foo')
            @depends('--foo')
            def foo(value):
                return value

            @depends('--help', foo)
            @imports('os')
            def bar(help, foo):
                return foo
        '''):
            self.lint_test()

        with self.assertRaisesFromLine(ConfigureError, 7) as e:
            with self.moz_configure('''
                option('--foo', help='foo')
                @depends('--foo')
                def foo(value):
                    return value

                @depends('--help', foo)
                def bar(help, foo):
                    return foo
            '''):
                self.lint_test()

        self.assertEquals(str(e.exception),
                          "The dependency on `--help` is unused")

        with self.assertRaisesFromLine(ConfigureError, 3) as e:
            with self.moz_configure('''
                option('--foo', help='foo')
                @depends('--foo')
                @imports('os')
                def foo(value):
                    return value

                @depends('--help', foo)
                @imports('os')
                def bar(help, foo):
                    return foo
            '''):
                self.lint_test()

        self.assertEquals(
            str(e.exception),
            "Missing '--help' dependency because `bar` depends on '--help' and `foo`")

        with self.assertRaisesFromLine(ConfigureError, 7) as e:
            with self.moz_configure('''
                @template
                def tmpl():
                    qux = 42

                    option('--foo', help='foo')
                    @depends('--foo')
                    def foo(value):
                        qux
                        return value

                    @depends('--help', foo)
                    @imports('os')
                    def bar(help, foo):
                        return foo
                tmpl()
            '''):
                self.lint_test()

        self.assertEquals(
            str(e.exception),
            "Missing '--help' dependency because `bar` depends on '--help' and `foo`")

        with self.moz_configure('''
            option('--foo', help='foo')
            @depends('--foo')
            def foo(value):
                return value

            include(foo)
        '''):
            self.lint_test()

        with self.assertRaisesFromLine(ConfigureError, 3) as e:
            with self.moz_configure('''
                option('--foo', help='foo')
                @depends('--foo')
                @imports('os')
                def foo(value):
                    return value

                include(foo)
            '''):
                self.lint_test()

        self.assertEquals(str(e.exception),
                          "Missing '--help' dependency")

        with self.assertRaisesFromLine(ConfigureError, 3) as e:
            with self.moz_configure('''
                option('--foo', help='foo')
                @depends('--foo')
                @imports('os')
                def foo(value):
                    return value

                @depends(foo)
                def bar(value):
                    return value

                include(bar)
            '''):
                self.lint_test()

        self.assertEquals(str(e.exception),
                          "Missing '--help' dependency")

        with self.assertRaisesFromLine(ConfigureError, 3) as e:
            with self.moz_configure('''
                option('--foo', help='foo')
                @depends('--foo')
                @imports('os')
                def foo(value):
                    return value

                option('--bar', help='bar', when=foo)
            '''):
                self.lint_test()

        self.assertEquals(str(e.exception),
                          "Missing '--help' dependency")

        # This would have failed with "Missing '--help' dependency"
        # in the past, because of the reference to the builtin False.
        with self.moz_configure('''
            option('--foo', help='foo')
            @depends('--foo')
            def foo(value):
                return False or value

            option('--bar', help='bar', when=foo)
        '''):
            self.lint_test()

        # However, when something that is normally a builtin is overridden,
        # we should still want the dependency on --help.
        with self.assertRaisesFromLine(ConfigureError, 7) as e:
            with self.moz_configure('''
                @template
                def tmpl():
                    sorted = 42

                    option('--foo', help='foo')
                    @depends('--foo')
                    def foo(value):
                        return sorted

                    option('--bar', help='bar', when=foo)
                tmpl()
            '''):
                self.lint_test()

        self.assertEquals(str(e.exception),
                          "Missing '--help' dependency")

        # There is a default restricted `os` module when there is no explicit
        # @imports, and it's fine to use it without a dependency on --help.
        with self.moz_configure('''
            option('--foo', help='foo')
            @depends('--foo')
            def foo(value):
                os
                return value

            include(foo)
        '''):
            self.lint_test()

        with self.assertRaisesFromLine(ConfigureError, 3) as e:
            with self.moz_configure('''
                option('--foo', help='foo')
                @depends('--foo')
                def foo(value):
                    return

                include(foo)
            '''):
                self.lint_test()

        self.assertEquals(str(e.exception),
                          "The dependency on `--foo` is unused")

        with self.assertRaisesFromLine(ConfigureError, 5) as e:
            with self.moz_configure('''
                @depends(when=True)
                def bar():
                    return
                @depends(bar)
                def foo(value):
                    return

                include(foo)
            '''):
                self.lint_test()

        self.assertEquals(str(e.exception),
                          "The dependency on `bar` is unused")

        with self.assertRaisesFromLine(ConfigureError, 2) as e:
            with self.moz_configure('''
                @depends(depends(when=True)(lambda: None))
                def foo(value):
                    return

                include(foo)
            '''):
                self.lint_test()

        self.assertEquals(str(e.exception),
                          "The dependency on `<lambda>` is unused")

        with self.assertRaisesFromLine(ConfigureError, 9) as e:
            with self.moz_configure('''
                @template
                def tmpl():
                    @depends(when=True)
                    def bar():
                        return
                    return bar
                qux = tmpl()
                @depends(qux)
                def foo(value):
                    return

                include(foo)
            '''):
                self.lint_test()

        self.assertEquals(str(e.exception),
                          "The dependency on `qux` is unused")

    def test_default_enable(self):
        # --enable-* with default=True is not allowed.
        with self.moz_configure('''
            option('--enable-foo', default=False, help='foo')
        '''):
            self.lint_test()
        with self.assertRaisesFromLine(ConfigureError, 2) as e:
            with self.moz_configure('''
                option('--enable-foo', default=True, help='foo')
            '''):
                self.lint_test()
        self.assertEquals(str(e.exception),
                          '--disable-foo should be used instead of '
                          '--enable-foo with default=True')

    def test_default_disable(self):
        # --disable-* with default=False is not allowed.
        with self.moz_configure('''
            option('--disable-foo', default=True, help='foo')
        '''):
            self.lint_test()
        with self.assertRaisesFromLine(ConfigureError, 2) as e:
            with self.moz_configure('''
                option('--disable-foo', default=False, help='foo')
            '''):
                self.lint_test()
        self.assertEquals(str(e.exception),
                          '--enable-foo should be used instead of '
                          '--disable-foo with default=False')

    def test_default_with(self):
        # --with-* with default=True is not allowed.
        with self.moz_configure('''
            option('--with-foo', default=False, help='foo')
        '''):
            self.lint_test()
        with self.assertRaisesFromLine(ConfigureError, 2) as e:
            with self.moz_configure('''
                option('--with-foo', default=True, help='foo')
            '''):
                self.lint_test()
        self.assertEquals(str(e.exception),
                          '--without-foo should be used instead of '
                          '--with-foo with default=True')

    def test_default_without(self):
        # --without-* with default=False is not allowed.
        with self.moz_configure('''
            option('--without-foo', default=True, help='foo')
        '''):
            self.lint_test()
        with self.assertRaisesFromLine(ConfigureError, 2) as e:
            with self.moz_configure('''
                option('--without-foo', default=False, help='foo')
            '''):
                self.lint_test()
        self.assertEquals(str(e.exception),
                          '--with-foo should be used instead of '
                          '--without-foo with default=False')

    def test_default_func(self):
        # Help text for an option with variable default should contain
        # {enable|disable} rule.
        with self.moz_configure('''
            option(env='FOO', help='foo')
            option('--enable-bar', default=depends('FOO')(lambda x: bool(x)),
                   help='{Enable|Disable} bar')
        '''):
            self.lint_test()
        with self.assertRaisesFromLine(ConfigureError, 4) as e:
            with self.moz_configure('''
                option(env='FOO', help='foo')
                option('--enable-bar', default=depends('FOO')(lambda x: bool(x)),
                       help='Enable bar')
            '''):
                self.lint_test()
        self.assertEquals(str(e.exception),
                          '`help` should contain "{Enable|Disable}" because of '
                          'non-constant default')

    def test_undefined_global(self):
        with self.assertRaisesFromLine(NameError, 6) as e:
            with self.moz_configure('''
                option(env='FOO', help='foo')
                @depends('FOO')
                def foo(value):
                    if value:
                        return unknown
                    return value
            '''):
                self.lint_test()

        self.assertEquals(str(e.exception),
                          "global name 'unknown' is not defined")


if __name__ == '__main__':
    main()
