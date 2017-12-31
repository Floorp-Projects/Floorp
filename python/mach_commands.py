# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import logging
import os
import sys
import tempfile

from concurrent.futures import (
    ThreadPoolExecutor,
    as_completed,
    thread,
)

import mozinfo
from manifestparser import TestManifest
from manifestparser import filters as mpf

import mozpack.path as mozpath
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
    @CommandArgument('--no-virtualenv', action='store_true',
                     help='Do not set up a virtualenv')
    @CommandArgument('args', nargs=argparse.REMAINDER)
    def python(self, no_virtualenv, args):
        # Avoid logging the command
        self.log_manager.terminal_handler.setLevel(logging.CRITICAL)

        # Note: subprocess requires native strings in os.environ on Windows.
        append_env = {
            b'PYTHONDONTWRITEBYTECODE': str('1'),
        }

        if no_virtualenv:
            python_path = sys.executable
            append_env[b'PYTHONPATH'] = os.pathsep.join(sys.path)
        else:
            self._activate_virtualenv()
            python_path = self.virtualenv_manager.python_path

        return self.run_process([python_path] + args,
                                pass_thru=True,  # Allow user to run Python interactively.
                                ensure_exit_code=False,  # Don't throw on non-zero exit code.
                                append_env=append_env)

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
    @CommandArgument('-j', '--jobs',
                     default=1,
                     type=int,
                     help='Number of concurrent jobs to run. Default is 1.')
    @CommandArgument('--subsuite',
                     default=None,
                     help=('Python subsuite to run. If not specified, all subsuites are run. '
                           'Use the string `default` to only run tests without a subsuite.'))
    @CommandArgument('tests', nargs='*',
                     metavar='TEST',
                     help=('Tests to run. Each test can be a single file or a directory. '
                           'Default test resolution relies on PYTHON_UNITTEST_MANIFESTS.'))
    def python_test(self, *args, **kwargs):
        try:
            tempdir = os.environ[b'PYTHON_TEST_TMP'] = str(tempfile.mkdtemp(suffix='-python-test'))
            return self.run_python_tests(*args, **kwargs)
        finally:
            import mozfile
            mozfile.remove(tempdir)

    def run_python_tests(self,
                         tests=[],
                         test_objects=None,
                         subsuite=None,
                         verbose=False,
                         stop=False,
                         jobs=1):
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
        if test_objects is None:
            from moztest.resolve import TestResolver
            resolver = self._spawn(TestResolver)
            if tests:
                # If we were given test paths, try to find tests matching them.
                test_objects = resolver.resolve_tests(paths=tests,
                                                      flavor='python')
            else:
                # Otherwise just run everything in PYTHON_UNITTEST_MANIFESTS
                test_objects = resolver.resolve_tests(flavor='python')

        mp = TestManifest()
        mp.tests.extend(test_objects)

        filters = []
        if subsuite == 'default':
            filters.append(mpf.subsuite(None))
        elif subsuite:
            filters.append(mpf.subsuite(subsuite))

        tests = mp.active_tests(filters=filters, disabled=False, **mozinfo.info)

        if not tests:
            submsg = "for subsuite '{}' ".format(subsuite) if subsuite else ""
            message = "TEST-UNEXPECTED-FAIL | No tests collected " + \
                      "{}(Not in PYTHON_UNITTEST_MANIFESTS?)".format(submsg)
            self.log(logging.WARN, 'python-test', {}, message)
            return 1

        parallel = []
        sequential = []
        for test in tests:
            if test.get('sequential'):
                sequential.append(test)
            else:
                parallel.append(test)

        self.jobs = jobs
        self.terminate = False
        self.verbose = verbose

        return_code = 0

        def on_test_finished(result):
            output, ret, test_path = result

            for line in output:
                self.log(logging.INFO, 'python-test', {'line': line.rstrip()}, '{line}')

            if ret and not return_code:
                self.log(logging.ERROR, 'python-test', {'test_path': test_path, 'ret': ret},
                         'Setting retcode to {ret} from {test_path}')
            return return_code or ret

        with ThreadPoolExecutor(max_workers=self.jobs) as executor:
            futures = [executor.submit(self._run_python_test, test['path'])
                       for test in parallel]

            try:
                for future in as_completed(futures):
                    return_code = on_test_finished(future.result())
            except KeyboardInterrupt:
                # Hack to force stop currently running threads.
                # https://gist.github.com/clchiou/f2608cbe54403edb0b13
                executor._threads.clear()
                thread._threads_queues.clear()
                raise

        for test in sequential:
            return_code = on_test_finished(self._run_python_test(test['path']))

        self.log(logging.INFO, 'python-test', {'return_code': return_code},
                 'Return code from mach python-test: {return_code}')
        return return_code

    def _run_python_test(self, test_path):
        from mozprocess import ProcessHandler

        output = []

        def _log(line):
            # Buffer messages if more than one worker to avoid interleaving
            if self.jobs > 1:
                output.append(line)
            else:
                self.log(logging.INFO, 'python-test', {'line': line.rstrip()}, '{line}')

        file_displayed_test = []  # used as boolean

        def _line_handler(line):
            if not file_displayed_test:
                output = ('Ran' in line or 'collected' in line or
                          line.startswith('TEST-'))
                if output:
                    file_displayed_test.append(True)

            # Hack to make sure treeherder highlights pytest failures
            if 'FAILED' in line.rsplit(' ', 1)[-1]:
                line = line.replace('FAILED', 'TEST-UNEXPECTED-FAIL')

            _log(line)

        _log(test_path)
        cmd = [self.virtualenv_manager.python_path, test_path]
        env = os.environ.copy()
        env[b'PYTHONDONTWRITEBYTECODE'] = b'1'

        proc = ProcessHandler(cmd, env=env, processOutputLine=_line_handler, storeOutput=False)
        proc.run()

        return_code = proc.wait()

        if not file_displayed_test:
            _log('TEST-UNEXPECTED-FAIL | No test output (missing mozunit.main() '
                 'call?): {}'.format(test_path))

        if self.verbose:
            if return_code != 0:
                _log('Test failed: {}'.format(test_path))
            else:
                _log('Test passed: {}'.format(test_path))

        return output, return_code, test_path
