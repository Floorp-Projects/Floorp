# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from StringIO import StringIO
import os
import unittest

from mozunit import main

from mozbuild.configure import (
    ConfigureError,
    ConfigureSandbox,
)

from buildconfig import topsrcdir


class FindProgramSandbox(ConfigureSandbox):
    def __init__(self, *args, **kwargs):
        super(FindProgramSandbox, self).__init__(*args, **kwargs)

        # We could define self.find_program_impl and have it automatically
        # declared, but then it wouldn't be available in the tested templates.
        # We also need to use super().__setitem__ because ours would do
        # nothing.
        super(FindProgramSandbox, self).__setitem__(
            'find_program', self.template_impl(self.find_program))

    PROGRAMS = {
        'known-a': '/usr/bin/known-a',
        'known-b': '/usr/local/bin/known-b',
        'known c': '/home/user/bin/known c',
    }

    for p in PROGRAMS.values():
        PROGRAMS[p] = p

    @staticmethod
    def find_program(prog):
        return FindProgramSandbox.PROGRAMS.get(prog)

    def __setitem__(self, key, value):
        # Avoid util.configure overwriting our mock find_program
        if key == 'find_program':
            return

        super(FindProgramSandbox, self).__setitem__(key, value)


class TestChecksConfigure(unittest.TestCase):
    def get_result(self, command='', args=[], environ={},
                   prog='/bin/configure'):
        config = {}
        out = StringIO()
        sandbox = FindProgramSandbox(config, environ, [prog] + args, out, out)
        base_dir = os.path.join(topsrcdir, 'build', 'moz.configure')
        sandbox.exec_file(os.path.join(base_dir, 'util.configure'))
        sandbox.exec_file(os.path.join(base_dir, 'checks.configure'))

        status = 0
        try:
            exec(command, sandbox)
        except SystemExit as e:
            status = e.code

        return config, out.getvalue(), status

    def test_check_prog(self):
        config, out, status = self.get_result(
            'check_prog("FOO", ("known-a",))')
        self.assertEqual(status, 0)
        self.assertEqual(config, {'FOO': '/usr/bin/known-a'})
        self.assertEqual(out, 'checking for foo... /usr/bin/known-a\n')

        config, out, status = self.get_result(
            'check_prog("FOO", ("unknown", "known-b", "known c"))')
        self.assertEqual(status, 0)
        self.assertEqual(config, {'FOO': '/usr/local/bin/known-b'})
        self.assertEqual(out, 'checking for foo... /usr/local/bin/known-b\n')

        config, out, status = self.get_result(
            'check_prog("FOO", ("unknown", "unknown-2", "known c"))')
        self.assertEqual(status, 0)
        self.assertEqual(config, {'FOO': '/home/user/bin/known c'})
        self.assertEqual(out, "checking for foo... '/home/user/bin/known c'\n")

        config, out, status = self.get_result(
            'check_prog("FOO", ("unknown",))')
        self.assertEqual(status, 1)
        self.assertEqual(config, {})
        self.assertEqual(out, 'checking for foo... not found\n'
                              'DEBUG: foo: Trying unknown\n'
                              'ERROR: Cannot find foo\n')

        config, out, status = self.get_result(
            'check_prog("FOO", ("unknown", "unknown-2", "unknown 3"))')
        self.assertEqual(status, 1)
        self.assertEqual(config, {})
        self.assertEqual(out, 'checking for foo... not found\n'
                              'DEBUG: foo: Trying unknown\n'
                              'DEBUG: foo: Trying unknown-2\n'
                              "DEBUG: foo: Trying 'unknown 3'\n"
                              'ERROR: Cannot find foo\n')

        config, out, status = self.get_result(
            'check_prog("FOO", ("unknown", "unknown-2", "unknown 3"), '
            'allow_missing=True)')
        self.assertEqual(status, 0)
        self.assertEqual(config, {'FOO': ':'})
        self.assertEqual(out, 'checking for foo... not found\n')

    def test_check_prog_with_args(self):
        config, out, status = self.get_result(
            'check_prog("FOO", ("unknown", "known-b", "known c"))',
            ['FOO=known-a'])
        self.assertEqual(status, 0)
        self.assertEqual(config, {'FOO': '/usr/bin/known-a'})
        self.assertEqual(out, 'checking for foo... /usr/bin/known-a\n')

        config, out, status = self.get_result(
            'check_prog("FOO", ("unknown", "known-b", "known c"))',
            ['FOO=/usr/bin/known-a'])
        self.assertEqual(status, 0)
        self.assertEqual(config, {'FOO': '/usr/bin/known-a'})
        self.assertEqual(out, 'checking for foo... /usr/bin/known-a\n')

        config, out, status = self.get_result(
            'check_prog("FOO", ("unknown", "known-b", "known c"))',
            ['FOO=/usr/local/bin/known-a'])
        self.assertEqual(status, 1)
        self.assertEqual(config, {})
        self.assertEqual(out, 'checking for foo... not found\n'
                              'DEBUG: foo: Trying /usr/local/bin/known-a\n'
                              'ERROR: Cannot find foo\n')

        config, out, status = self.get_result(
            'check_prog("FOO", ("unknown",))',
            ['FOO=known c'])
        self.assertEqual(status, 0)
        self.assertEqual(config, {'FOO': '/home/user/bin/known c'})
        self.assertEqual(out, "checking for foo... '/home/user/bin/known c'\n")

        config, out, status = self.get_result(
            'check_prog("FOO", ("unknown", "unknown-2", "unknown 3"), '
            'allow_missing=True)', ['FOO=unknown'])
        self.assertEqual(status, 1)
        self.assertEqual(config, {})
        self.assertEqual(out, 'checking for foo... not found\n'
                              'DEBUG: foo: Trying unknown\n'
                              'ERROR: Cannot find foo\n')

    def test_check_prog_what(self):
        config, out, status = self.get_result(
            'check_prog("CC", ("known-a",), what="the target C compiler")')
        self.assertEqual(status, 0)
        self.assertEqual(config, {'CC': '/usr/bin/known-a'})
        self.assertEqual(out, 'checking for the target C compiler... /usr/bin/known-a\n')

        config, out, status = self.get_result(
            'check_prog("CC", ("unknown", "unknown-2", "unknown 3"),'
            '           what="the target C compiler")')
        self.assertEqual(status, 1)
        self.assertEqual(config, {})
        self.assertEqual(out, 'checking for the target C compiler... not found\n'
                              'DEBUG: cc: Trying unknown\n'
                              'DEBUG: cc: Trying unknown-2\n'
                              "DEBUG: cc: Trying 'unknown 3'\n"
                              'ERROR: Cannot find the target C compiler\n')

    def test_check_prog_configure_error(self):
        with self.assertRaises(ConfigureError) as e:
            self.get_result('check_prog("FOO", "foo")')

        self.assertEqual(e.exception.message,
                         'progs should be a list or tuple!')


if __name__ == '__main__':
    main()
