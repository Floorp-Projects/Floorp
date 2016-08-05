# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import __main__
import argparse
import logging
import mozpack.path as mozpath
import os

from mozbuild.base import (
    MachCommandBase,
)

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)


@CommandProvider
class MachCommands(MachCommandBase):
    @Command('python', category='devenv',
        description='Run Python.')
    @CommandArgument('args', nargs=argparse.REMAINDER)
    def python(self, args):
        # Avoid logging the command
        self.log_manager.terminal_handler.setLevel(logging.CRITICAL)

        self._activate_virtualenv()

        return self.run_process([self.virtualenv_manager.python_path] + args,
            pass_thru=True,  # Allow user to run Python interactively.
            ensure_exit_code=False,  # Don't throw on non-zero exit code.
            # Note: subprocess requires native strings in os.environ on Windows
            append_env={b'PYTHONDONTWRITEBYTECODE': str('1')})

    @Command('python-test', category='testing',
        description='Run Python unit tests with an appropriate test runner.')
    @CommandArgument('--verbose',
        default=False,
        action='store_true',
        help='Verbose output.')
    @CommandArgument('--stop',
        default=False,
        action='store_true',
        help='Stop running tests after the first error or failure.')
    @CommandArgument('--path-only',
        default=False,
        action='store_true',
        help=('Collect all tests under given path instead of default '
              'test resolution. Supports pytest-style tests.'))
    @CommandArgument('tests', nargs='*',
        metavar='TEST',
        help=('Tests to run. Each test can be a single file or a directory. '
              'Default test resolution relies on PYTHON_UNIT_TESTS.'))
    def python_test(self,
                    tests=[],
                    test_objects=None,
                    subsuite=None,
                    verbose=False,
                    path_only=False,
                    stop=False):
        self._activate_virtualenv()

        def find_tests_by_path():
            import glob
            files = []
            for t in tests:
                if t.endswith('.py') and os.path.isfile(t):
                    files.append(t)
                elif os.path.isdir(t):
                    for root, _, _ in os.walk(t):
                        files += glob.glob(mozpath.join(root, 'test*.py'))
                        files += glob.glob(mozpath.join(root, 'unit*.py'))
                else:
                    self.log(logging.WARN, 'python-test',
                                 {'test': t},
                                 'TEST-UNEXPECTED-FAIL | Invalid test: {test}')
                    if stop:
                        break
            return files

        # Python's unittest, and in particular discover, has problems with
        # clashing namespaces when importing multiple test modules. What follows
        # is a simple way to keep environments separate, at the price of
        # launching Python multiple times. Most tests are run via mozunit,
        # which produces output in the format Mozilla infrastructure expects.
        # Some tests are run via pytest.
        return_code = 0
        found_tests = False
        if test_objects is None:
            # If we're not being called from `mach test`, do our own
            # test resolution.
            if path_only:
                if tests:
                    test_objects = [{'path': p} for p in find_tests_by_path()]
                else:
                    self.log(logging.WARN, 'python-test', {},
                             'TEST-UNEXPECTED-FAIL | No tests specified')
                    test_objects = []
            else:
                from mozbuild.testing import TestResolver
                resolver = self._spawn(TestResolver)
                if tests:
                    # If we were given test paths, try to find tests matching them.
                    test_objects = resolver.resolve_tests(paths=tests,
                                                          flavor='python')
                else:
                    # Otherwise just run everything in PYTHON_UNIT_TESTS
                    test_objects = resolver.resolve_tests(flavor='python')

        for test in test_objects:
            found_tests = True
            f = test['path']
            file_displayed_test = []  # Used as a boolean.

            def _line_handler(line):
                if not file_displayed_test:
                    output = ('Ran' in line or 'collected' in line or
                              line.startswith('TEST-'))
                    if output:
                        file_displayed_test.append(True)

            inner_return_code = self.run_process(
                [self.virtualenv_manager.python_path, f],
                ensure_exit_code=False,  # Don't throw on non-zero exit code.
                log_name='python-test',
                # subprocess requires native strings in os.environ on Windows
                append_env={b'PYTHONDONTWRITEBYTECODE': str('1')},
                line_handler=_line_handler)
            return_code += inner_return_code

            if not file_displayed_test:
                self.log(logging.WARN, 'python-test', {'file': f},
                         'TEST-UNEXPECTED-FAIL | No test output (missing mozunit.main() call?): {file}')

            if verbose:
                if inner_return_code != 0:
                    self.log(logging.INFO, 'python-test', {'file': f},
                             'Test failed: {file}')
                else:
                    self.log(logging.INFO, 'python-test', {'file': f},
                             'Test passed: {file}')
            if stop and return_code > 0:
                return 1

        if not found_tests:
            message = 'TEST-UNEXPECTED-FAIL | No tests collected'
            if not path_only:
                 message += ' (Not in PYTHON_UNIT_TESTS? Try --path-only?)'
            self.log(logging.WARN, 'python-test', {}, message)
            return 1

        return 0 if return_code == 0 else 1
