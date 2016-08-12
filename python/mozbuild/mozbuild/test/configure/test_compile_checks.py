# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import textwrap
import unittest
import mozpack.path as mozpath

from StringIO import StringIO

from buildconfig import topsrcdir
from common import ConfigureTestSandbox
from mozbuild.util import exec_
from mozunit import main
from test_toolchain_helpers import FakeCompiler


class BaseCompileChecks(unittest.TestCase):
    def get_mock_compiler(self, expected_test_content=None, expected_flags=None):
        expected_flags = expected_flags or []
        def mock_compiler(stdin, args):
            args, test_file = args[:-1], args[-1]
            self.assertIn('-c', args)
            for flag in expected_flags:
                self.assertIn(flag, args)

            if expected_test_content:
                with open(test_file) as fh:
                    test_content = fh.read()
                self.assertEqual(test_content, expected_test_content)

            return FakeCompiler()(None, args)
        return mock_compiler

    def do_compile_test(self, command, expected_test_content=None,
                        expected_flags=None):

        paths = {
            os.path.abspath('/usr/bin/mockcc'): self.get_mock_compiler(
                expected_test_content=expected_test_content,
                expected_flags=expected_flags),
        }

        base_dir = os.path.join(topsrcdir, 'build', 'moz.configure')

        mock_compiler_defs = textwrap.dedent('''\
            @depends('--help')
            def extra_toolchain_flags(_):
                return []

            include('%s/compilers-util.configure')

            @compiler_class
            @depends('--help')
            def c_compiler(_):
                return namespace(
                    flags=[],
                    type='gcc',
                    compiler=os.path.abspath('/usr/bin/mockcc'),
                    wrapper=[],
                    language='C',
                )

            @compiler_class
            @depends('--help')
            def cxx_compiler(_):
                return namespace(
                    flags=[],
                    type='gcc',
                    compiler=os.path.abspath('/usr/bin/mockcc'),
                    wrapper=[],
                    language='C++',
                )
        ''' % mozpath.normsep(base_dir))

        config = {}
        out = StringIO()
        sandbox = ConfigureTestSandbox(paths, config, {}, ['/bin/configure'],
                                       out, out)
        sandbox.include_file(os.path.join(base_dir, 'util.configure'))
        sandbox.include_file(os.path.join(base_dir, 'checks.configure'))
        exec_(mock_compiler_defs, sandbox)
        sandbox.include_file(os.path.join(base_dir, 'compile-checks.configure'))

        status = 0
        try:
            exec_(command, sandbox)
            sandbox.run()
        except SystemExit as e:
            status = e.code

        return config, out.getvalue(), status


class TestHeaderChecks(BaseCompileChecks):
    def test_try_compile_include(self):
        expected_test_content = textwrap.dedent('''\
          #include <foo.h>
          #include <bar.h>
          int
          main(void)
          {

            ;
            return 0;
          }
        ''')

        cmd = textwrap.dedent('''\
            try_compile(['foo.h', 'bar.h'], language='C')
        ''')

        config, out, status = self.do_compile_test(cmd, expected_test_content)
        self.assertEqual(status, 0)
        self.assertEqual(config, {})

    def test_try_compile_flags(self):
        expected_flags = ['--extra', '--flags']

        cmd = textwrap.dedent('''\
            try_compile(language='C++', flags=['--flags', '--extra'])
        ''')

        config, out, status = self.do_compile_test(cmd, expected_flags=expected_flags)
        self.assertEqual(status, 0)
        self.assertEqual(config, {})

    def test_try_compile_failure(self):
        cmd = textwrap.dedent('''\
            have_fn = try_compile(body='somefn();', flags=['-funknown-flag'])
            set_config('HAVE_SOMEFN', have_fn)

            have_another = try_compile(body='anotherfn();', language='C')
            set_config('HAVE_ANOTHERFN', have_another)
        ''')

        config, out, status = self.do_compile_test(cmd)
        self.assertEqual(status, 0)
        self.assertEqual(config, {
            'HAVE_ANOTHERFN': True,
        })

    def test_try_compile_msg(self):
        cmd = textwrap.dedent('''\
            known_flag = try_compile(language='C++', flags=['-fknown-flag'],
                                     check_msg='whether -fknown-flag works')
            set_config('HAVE_KNOWN_FLAG', known_flag)
        ''')
        config, out, status = self.do_compile_test(cmd)
        self.assertEqual(status, 0)
        self.assertEqual(config, {'HAVE_KNOWN_FLAG': True})
        self.assertEqual(out, textwrap.dedent('''\
            checking whether -fknown-flag works... yes
        '''))

    def test_check_header(self):
        expected_test_content = textwrap.dedent('''\
          #include <foo.h>
          int
          main(void)
          {

            ;
            return 0;
          }
        ''')

        cmd = textwrap.dedent('''\
            check_header('foo.h')
        ''')

        config, out, status = self.do_compile_test(cmd,
                                                   expected_test_content=expected_test_content)
        self.assertEqual(status, 0)
        self.assertEqual(config, {'DEFINES': {'HAVE_FOO_H': True}})
        self.assertEqual(out, textwrap.dedent('''\
            checking for foo.h... yes
        '''))

    def test_check_header_include(self):
        expected_test_content = textwrap.dedent('''\
          #include <std.h>
          #include <bar.h>
          #include <foo.h>
          int
          main(void)
          {

            ;
            return 0;
          }
        ''')

        cmd = textwrap.dedent('''\
           have_foo = check_header('foo.h', includes=['std.h', 'bar.h'])
           set_config('HAVE_FOO_H', have_foo)
        ''')

        config, out, status = self.do_compile_test(cmd,
                                                   expected_test_content=expected_test_content)

        self.assertEqual(status, 0)
        self.assertEqual(config, {
            'HAVE_FOO_H': True,
            'DEFINES': {
                'HAVE_FOO_H': True,
            }
        })
        self.assertEqual(out, textwrap.dedent('''\
            checking for foo.h... yes
        '''))

    def test_check_headers_multiple(self):
        cmd = textwrap.dedent('''\
            baz_bar, quux_bar = check_headers('baz/foo-bar.h', 'baz-quux/foo-bar.h')
            set_config('HAVE_BAZ_BAR', baz_bar)
            set_config('HAVE_QUUX_BAR', quux_bar)
        ''')

        config, out, status = self.do_compile_test(cmd)
        self.assertEqual(status, 0)
        self.assertEqual(config, {
            'HAVE_BAZ_BAR': True,
            'HAVE_QUUX_BAR': True,
            'DEFINES': {
                'HAVE_BAZ_FOO_BAR_H': True,
                'HAVE_BAZ_QUUX_FOO_BAR_H': True,
            }
        })
        self.assertEqual(out, textwrap.dedent('''\
            checking for baz/foo-bar.h... yes
            checking for baz-quux/foo-bar.h... yes
        '''))

    def test_check_headers_not_found(self):

        cmd = textwrap.dedent('''\
            baz_bar, quux_bar = check_headers('baz/foo-bar.h', 'baz-quux/foo-bar.h',
                                              flags=['-funknown-flag'])
            set_config('HAVE_BAZ_BAR', baz_bar)
            set_config('HAVE_QUUX_BAR', quux_bar)
        ''')

        config, out, status = self.do_compile_test(cmd)
        self.assertEqual(status, 0)
        self.assertEqual(config, {'DEFINES': {}})
        self.assertEqual(out, textwrap.dedent('''\
            checking for baz/foo-bar.h... no
            checking for baz-quux/foo-bar.h... no
        '''))


class TestWarningChecks(BaseCompileChecks):
    def get_warnings(self):
        return textwrap.dedent('''\
            set_config('_WARNINGS_CFLAGS', warnings_cflags)
            set_config('_WARNINGS_CXXFLAGS', warnings_cxxflags)
        ''')

    def test_check_and_add_gcc_warning(self):
        for flag, expected_flags in (
            ('-Wfoo', ['-Werror', '-Wfoo']),
            ('-Wno-foo', ['-Werror', '-Wfoo']),
            ('-Werror=foo', ['-Werror=foo']),
            ('-Wno-error=foo', ['-Wno-error=foo']),
        ):
            cmd = textwrap.dedent('''\
                check_and_add_gcc_warning('%s')
            ''' % flag) + self.get_warnings()

            config, out, status = self.do_compile_test(
                cmd, expected_flags=expected_flags)
            self.assertEqual(status, 0)
            self.assertEqual(config, {
                '_WARNINGS_CFLAGS': [flag],
                '_WARNINGS_CXXFLAGS': [flag],
            })
            self.assertEqual(out, textwrap.dedent('''\
                checking whether the C compiler supports {flag}... yes
                checking whether the C++ compiler supports {flag}... yes
            '''.format(flag=flag)))

    def test_check_and_add_gcc_warning_one(self):
        cmd = textwrap.dedent('''\
            check_and_add_gcc_warning('-Wfoo', cxx_compiler)
        ''') + self.get_warnings()

        config, out, status = self.do_compile_test(cmd)
        self.assertEqual(status, 0)
        self.assertEqual(config, {
            '_WARNINGS_CFLAGS': [],
            '_WARNINGS_CXXFLAGS': ['-Wfoo'],
        })
        self.assertEqual(out, textwrap.dedent('''\
            checking whether the C++ compiler supports -Wfoo... yes
        '''))

    def test_check_and_add_gcc_warning_when(self):
        cmd = textwrap.dedent('''\
            @depends('--help')
            def never(_):
                return False
            check_and_add_gcc_warning('-Wfoo', cxx_compiler, when=never)
        ''') + self.get_warnings()

        config, out, status = self.do_compile_test(cmd)
        self.assertEqual(status, 0)
        self.assertEqual(config, {
            '_WARNINGS_CFLAGS': [],
            '_WARNINGS_CXXFLAGS': [],
        })
        self.assertEqual(out, '')

        cmd = textwrap.dedent('''\
            @depends('--help')
            def always(_):
                return True
            check_and_add_gcc_warning('-Wfoo', cxx_compiler, when=always)
        ''') + self.get_warnings()

        config, out, status = self.do_compile_test(cmd)
        self.assertEqual(status, 0)
        self.assertEqual(config, {
            '_WARNINGS_CFLAGS': [],
            '_WARNINGS_CXXFLAGS': ['-Wfoo'],
        })
        self.assertEqual(out, textwrap.dedent('''\
            checking whether the C++ compiler supports -Wfoo... yes
        '''))

    def test_add_gcc_warning(self):
        cmd = textwrap.dedent('''\
            add_gcc_warning('-Wfoo')
        ''') + self.get_warnings()

        config, out, status = self.do_compile_test(cmd)
        self.assertEqual(status, 0)
        self.assertEqual(config, {
            '_WARNINGS_CFLAGS': ['-Wfoo'],
            '_WARNINGS_CXXFLAGS': ['-Wfoo'],
        })
        self.assertEqual(out, '')

    def test_add_gcc_warning_one(self):
        cmd = textwrap.dedent('''\
            add_gcc_warning('-Wfoo', c_compiler)
        ''') + self.get_warnings()

        config, out, status = self.do_compile_test(cmd)
        self.assertEqual(status, 0)
        self.assertEqual(config, {
            '_WARNINGS_CFLAGS': ['-Wfoo'],
            '_WARNINGS_CXXFLAGS': [],
        })
        self.assertEqual(out, '')

    def test_add_gcc_warning_when(self):
        cmd = textwrap.dedent('''\
            @depends('--help')
            def never(_):
                return False
            add_gcc_warning('-Wfoo', c_compiler, when=never)
        ''') + self.get_warnings()

        config, out, status = self.do_compile_test(cmd)
        self.assertEqual(status, 0)
        self.assertEqual(config, {
            '_WARNINGS_CFLAGS': [],
            '_WARNINGS_CXXFLAGS': [],
        })
        self.assertEqual(out, '')

        cmd = textwrap.dedent('''\
            @depends('--help')
            def always(_):
                return True
            add_gcc_warning('-Wfoo', c_compiler, when=always)
        ''') + self.get_warnings()

        config, out, status = self.do_compile_test(cmd)
        self.assertEqual(status, 0)
        self.assertEqual(config, {
            '_WARNINGS_CFLAGS': ['-Wfoo'],
            '_WARNINGS_CXXFLAGS': [],
        })
        self.assertEqual(out, '')


if __name__ == '__main__':
    main()
