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
                return foo
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
                    return foo
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
                        return foo
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

        with self.assertRaises(ConfigureError) as e:
            with self.moz_configure('''
                option('--foo', help='foo')
                @depends('--foo')
                def foo(value):
                    return

                include(foo)
            '''):
                self.lint_test()

        self.assertEquals(e.exception.message,
                          "%s:3: The dependency on `--foo` is unused."
                          % mozpath.join(test_data_path, 'moz.configure'))

        with self.assertRaises(ConfigureError) as e:
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

        self.assertEquals(e.exception.message,
                          "%s:5: The dependency on `bar` is unused."
                          % mozpath.join(test_data_path, 'moz.configure'))

        with self.assertRaises(ConfigureError) as e:
            with self.moz_configure('''
                @depends(depends(when=True)(lambda: None))
                def foo(value):
                    return

                include(foo)
            '''):
                self.lint_test()

        self.assertEquals(e.exception.message,
                          "%s:2: The dependency on `<lambda>` is unused."
                          % mozpath.join(test_data_path, 'moz.configure'))

        with self.assertRaises(ConfigureError) as e:
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

        self.assertEquals(e.exception.message,
                          "%s:9: The dependency on `qux` is unused."
                          % mozpath.join(test_data_path, 'moz.configure'))


if __name__ == '__main__':
    main()
