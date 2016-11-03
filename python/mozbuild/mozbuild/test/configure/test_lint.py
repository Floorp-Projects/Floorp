# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from StringIO import StringIO
import os
import textwrap
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

    def test_depends_failures(self):
        with self.moz_configure('''
            option('--foo', help='foo')
            @depends('--foo')
            def foo(value):
                return value

            @depends('--help', foo)
            def bar(help, foo):
                return
        '''):
            self.lint_test()

        with self.assertRaises(ConfigureError) as e:
            with self.moz_configure('''
                option('--foo', help='foo')
                @depends('--foo')
                @imports('os')
                def foo(value):
                    return value

                @depends('--help', foo)
                def bar(help, foo):
                    return
            '''):
                self.lint_test()

        self.assertEquals(e.exception.message,
                          "`bar` depends on '--help' and `foo`. "
                          "`foo` must depend on '--help'")

        with self.assertRaises(ConfigureError) as e:
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
                    def bar(help, foo):
                        return
                tmpl()
            '''):
                self.lint_test()

        self.assertEquals(e.exception.message,
                          "`bar` depends on '--help' and `foo`. "
                          "`foo` must depend on '--help'")

        with self.assertRaises(ConfigureError) as e:
            with self.moz_configure('''
                @template
                @imports('os')
                def tmpl():
                    option('--foo', help='foo')
                    @depends('--foo')
                    def foo(value):
                        os
                        return value

                    @depends('--help', foo)
                    def bar(help, foo):
                        return
                tmpl()
            '''):
                self.lint_test()

        self.assertEquals(e.exception.message,
                          "`bar` depends on '--help' and `foo`. "
                          "`foo` must depend on '--help'")

        with self.moz_configure('''
            option('--foo', help='foo')
            @depends('--foo')
            def foo(value):
                return value

            include(foo)
        '''):
            self.lint_test()

        with self.assertRaises(ConfigureError) as e:
            with self.moz_configure('''
                option('--foo', help='foo')
                @depends('--foo')
                @imports('os')
                def foo(value):
                    return value

                include(foo)
            '''):
                self.lint_test()

        self.assertEquals(e.exception.message,
                          "Missing @depends for `foo`: '--help'")

        with self.assertRaises(ConfigureError) as e:
            with self.moz_configure('''
                @template
                @imports('os')
                def tmpl():
                    option('--foo', help='foo')
                    @depends('--foo')
                    def foo(value):
                        os
                        return value
                    return foo

                foo = tmpl()

                include(foo)
            '''):
                self.lint_test()

        self.assertEquals(e.exception.message,
                          "Missing @depends for `foo`: '--help'")


if __name__ == '__main__':
    main()
