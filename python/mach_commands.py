# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import logging
import os
import sys
import tempfile
from multiprocessing import cpu_count

from concurrent.futures import (
    ThreadPoolExecutor,
    as_completed,
    thread,
)

import mozinfo
from manifestparser import TestManifest
from manifestparser import filters as mpf

from mozbuild.base import (
    MachCommandBase,
)

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

here = os.path.abspath(os.path.dirname(__file__))


@CommandProvider
class MachCommands(MachCommandBase):
    @Command('python', category='devenv',
             description='Run Python.')
    @CommandArgument('--no-virtualenv', action='store_true',
                     help='Do not set up a virtualenv')
    @CommandArgument('--exec-file',
                     default=None,
                     help='Execute this Python file using `execfile`')
    @CommandArgument('args', nargs=argparse.REMAINDER)
    def python(self, no_virtualenv, exec_file, args):
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

        if exec_file:
            execfile(exec_file)
            return 0

        return self.run_process([python_path] + args,
                                pass_thru=True,  # Allow user to run Python interactively.
                                ensure_exit_code=False,  # Don't throw on non-zero exit code.
                                append_env=append_env)

    @Command('python-test', category='testing',
             description='Run Python unit tests with an appropriate test runner.')
    @CommandArgument('-v', '--verbose',
                     default=False,
                     action='store_true',
                     help='Verbose output.')
    @CommandArgument('--python',
                     default='2.7',
                     help='Version of Python for Pipenv to use. When given a '
                          'Python version, Pipenv will automatically scan your '
                          'system for a Python that matches that given version.')
    @CommandArgument('-j', '--jobs',
                     default=None,
                     type=int,
                     help='Number of concurrent jobs to run. Default is the number of CPUs '
                          'in the system.')
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
                         tests=None,
                         test_objects=None,
                         subsuite=None,
                         verbose=False,
                         jobs=None,
                         python=None,
                         **kwargs):
        python = python or self.virtualenv_manager.python_path
        self.activate_pipenv(pipfile=None, populate=True, python=python)

        if test_objects is None:
            from moztest.resolve import TestResolver
            resolver = self._spawn(TestResolver)
            # If we were given test paths, try to find tests matching them.
            test_objects = resolver.resolve_tests(paths=tests, flavor='python')
        else:
            # We've received test_objects from |mach test|. We need to ignore
            # the subsuite because python-tests don't use this key like other
            # harnesses do and |mach test| doesn't realize this.
            subsuite = None

        mp = TestManifest()
        mp.tests.extend(test_objects)

        filters = []
        if subsuite == 'default':
            filters.append(mpf.subsuite(None))
        elif subsuite:
            filters.append(mpf.subsuite(subsuite))

        tests = mp.active_tests(
            filters=filters,
            disabled=False,
            python=self.virtualenv_manager.version_info[0],
            **mozinfo.info)

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

        self.jobs = jobs or cpu_count()
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
            futures = [executor.submit(self._run_python_test, test)
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
            return_code = on_test_finished(self._run_python_test(test))

        self.log(logging.INFO, 'python-test', {'return_code': return_code},
                 'Return code from mach python-test: {return_code}')
        return return_code

    def _run_python_test(self, test):
        from mozprocess import ProcessHandler

        if test.get('requirements'):
            self.virtualenv_manager.install_pip_requirements(test['requirements'], quiet=True)

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

        _log(test['path'])
        cmd = [self.virtualenv_manager.python_path, test['path']]
        env = os.environ.copy()
        env[b'PYTHONDONTWRITEBYTECODE'] = b'1'

        proc = ProcessHandler(cmd, env=env, processOutputLine=_line_handler, storeOutput=False)
        proc.run()

        return_code = proc.wait()

        if not file_displayed_test:
            _log('TEST-UNEXPECTED-FAIL | No test output (missing mozunit.main() '
                 'call?): {}'.format(test['path']))

        if self.verbose:
            if return_code != 0:
                _log('Test failed: {}'.format(test['path']))
            else:
                _log('Test passed: {}'.format(test['path']))

        return output, return_code, test['path']
