# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import logging
import mozpack.path as mozpath
import os
import platform
import subprocess
import sys
import which

from mozbuild.base import (
    MachCommandBase,
)

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

ESLINT_NOT_FOUND_MESSAGE = '''
Could not find eslint!  We looked at the --binary option, at the ESLINT
environment variable, and then at your path.  Install eslint and needed plugins
with

mach eslint --setup

and try again.
'''.strip()

NODE_NOT_FOUND_MESSAGE = '''
nodejs is either not installed or is installed to a non-standard path.
Please install nodejs from https://nodejs.org and try again.

Valid installation paths:
'''.strip()

NPM_NOT_FOUND_MESSAGE = '''
Node Package Manager (npm) is either not installed or installed to a
non-standard path. Please install npm from https://nodejs.org (it comes as an
option in the node installation) and try again.

Valid installation paths:
'''.strip()

ESLINT_PROMPT = '''
Would you like to use eslint
'''.strip()

ESLINT_PLUGIN_MOZILLA_PROMPT = '''
eslint-plugin-mozilla is an eslint plugin containing rules that help enforce
JavaScript coding standards in the Mozilla project. Would you like to use this
plugin
'''.strip()

ESLINT_PLUGIN_REACT_PROMPT = '''
eslint-plugin-react is an eslint plugin containing rules that help React
developers follow strict guidelines. Would you like to install it
'''.strip()


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
        description='Run Python unit tests.')
    @CommandArgument('--verbose',
        default=False,
        action='store_true',
        help='Verbose output.')
    @CommandArgument('--stop',
        default=False,
        action='store_true',
        help='Stop running tests after the first error or failure.')
    @CommandArgument('tests', nargs='+',
        metavar='TEST',
        help='Tests to run. Each test can be a single file or a directory.')
    def python_test(self, tests, verbose=False, stop=False):
        self._activate_virtualenv()
        import glob

        # Python's unittest, and in particular discover, has problems with
        # clashing namespaces when importing multiple test modules. What follows
        # is a simple way to keep environments separate, at the price of
        # launching Python multiple times. This also runs tests via mozunit,
        # which produces output in the format Mozilla infrastructure expects.
        return_code = 0
        files = []
        # We search for files in both the current directory (for people running
        # from topsrcdir or cd'd into their test directory) and topsrcdir (to
        # support people running mach from the objdir).  The |break|s in the
        # loop below ensure that we don't run tests twice if we're running mach
        # from topsrcdir
        search_dirs = ['.', self.topsrcdir]
        last_search_dir = search_dirs[-1]
        for t in tests:
            for d in search_dirs:
                test = mozpath.join(d, t)
                if test.endswith('.py') and os.path.isfile(test):
                    files.append(test)
                    break
                elif os.path.isfile(test + '.py'):
                    files.append(test + '.py')
                    break
                elif os.path.isdir(test):
                    files += glob.glob(mozpath.join(test, 'test*.py'))
                    files += glob.glob(mozpath.join(test, 'unit*.py'))
                    break
                elif d == last_search_dir:
                    self.log(logging.WARN, 'python-test',
                             {'test': t},
                             'TEST-UNEXPECTED-FAIL | Invalid test: {test}')
                    if stop:
                        return 1

        for f in files:
            file_displayed_test = []  # Used as a boolean.

            def _line_handler(line):
                if not file_displayed_test and line.startswith('TEST-'):
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

        return 0 if return_code == 0 else 1

    @Command('eslint', category='devenv',
        description='Run eslint or help configure eslint for optimal development.')
    @CommandArgument('-s', '--setup', default=False, action='store_true',
        help='configure eslint for optimal development.')
    @CommandArgument('path', nargs='?', default='.',
        help='Path to files to lint, like "browser/components/loop" '
            'or "mobile/android". '
            'Defaults to the current directory if not given.')
    @CommandArgument('-e', '--ext', default='[.js,.jsm,.jsx]',
        help='Filename extensions to lint, default: "[.js,.jsm,.jsx]".')
    @CommandArgument('-b', '--binary', default=None,
        help='Path to eslint binary.')
    @CommandArgument('args', nargs=argparse.REMAINDER)  # Passed through to eslint.
    def eslint(self, setup, path, ext=None, binary=None, args=[]):
        '''Run eslint.'''

        if setup:
            return self.eslint_setup()

        if not binary:
            binary = os.environ.get('ESLINT', None)
            if not binary:
                try:
                    binary = which.which('eslint')
                except which.WhichError:
                    pass

        if not binary:
            print(ESLINT_NOT_FOUND_MESSAGE)
            return 1

        # The cwd below is unfortunate.  eslint --config=PATH/TO/.eslintrc works,
        # but --ignore-path=PATH/TO/.eslintignore treats paths as relative to
        # the current directory, rather than as relative to the location of
        # .eslintignore (see https://github.com/eslint/eslint/issues/1382).
        # mach commands always execute in the topsrcdir, so we could make all
        # paths in .eslint relative to the topsrcdir, but it's not clear if
        # that's a good choice for future eslint and IDE integrations.
        # Unfortunately, running after chdir does not print the full path to
        # files (convenient for opening with copy-and-paste).  In the meantime,
        # we just print the active path.

        self.log(logging.INFO, 'eslint', {'binary': binary, 'path': path},
            'Running {binary} in {path}')

        cmd_args = [binary,
            '--ext', ext,  # This keeps ext as a single argument.
        ] + args
        # Path must come after arguments.  Path is '.' due to cwd below.
        cmd_args += ['.']

        return self.run_process(cmd_args,
            cwd=path,
            pass_thru=True,  # Allow user to run eslint interactively.
            ensure_exit_code=False,  # Don't throw on non-zero exit code.
        )
    def eslint_setup(self, update_only=False):
        """Ensure eslint is optimally configured.

        This command will inspect your eslint configuration and
        guide you through an interactive wizard helping you configure
        eslint for optimal use on Mozilla projects.
        """
        sys.path.append(os.path.dirname(__file__))

        # At the very least we need node installed.
        nodePath = self.getNodeOrNpmPath("node")
        if not nodePath:
            return 1

        npmPath = self.getNodeOrNpmPath("npm")
        if not npmPath:
            return 1

        # Install eslint.
        print("Installing eslint...")
        with open(os.devnull, "w") as fnull:
            subprocess.call([npmPath, "install", "eslint", "-g"],
                            stdout=fnull, stderr=fnull)

        # Install eslint-plugin-mozilla.
        print("")
        print("Installing eslint-plugin-mozilla...")
        with open(os.devnull, "w") as fnull:
            subprocess.call([npmPath, "link"],
                            cwd="testing/eslint-plugin-mozilla",
                            stdout=fnull, stderr=fnull)

        # Install eslint-plugin-react.
        print("")
        print("Installing eslint-plugin-react...")
        with open(os.devnull, "w") as fnull:
            subprocess.call([npmPath, "install", "-g", "eslint-plugin-react"],
                            stdout=fnull, stderr=fnull)

    def getPossibleNodePathsWin(self):
        """
        Return possible nodejs paths on Windows.
        """
        if platform.system() != "Windows":
            return []

        return {
            "%s\\nodejs" % os.environ.get("SystemDrive"),
            os.path.join(os.environ.get("ProgramFiles"), "nodejs"),
            os.path.join(os.environ.get("PROGRAMW6432"), "nodejs"),
            os.path.join(os.environ.get("PROGRAMFILES"), "nodejs")
        }

    def getNodeOrNpmPath(self, filename):
        """
        Return the nodejs or npm path.
        """
        try:
            appPath = which.which(filename)
            return appPath
        except which.WhichError:
            pass

        if platform.system() == "Windows":
            try:
                for ext in ["", ".cmd", ".exe"]:
                    nodeOrNpmPath = which.which(filename + ext,
                                                path=self.getPossibleNodePathsWin())
                    if self.is_valid(nodeOrNpmPath):
                        return nodeOrNpmPath
            except which.WhichError:
                pass

        if filename == "node":
            print(NODE_NOT_FOUND_MESSAGE)
        elif filename == "npm":
            print(NPM_NOT_FOUND_MESSAGE)

        if platform.system() == "Windows":
            appPaths = self.getPossibleNodePathsWin()

            for p in appPaths:
                print("  - " + p)
        elif platform.system() == "Darwin":
            print("  - /usr/local/bin/node")
        elif platform.system() == "Linux":
            print("  - /usr/bin/nodejs")

        return None

    def is_valid(self, path):
        try:
            with open(os.devnull, "w") as fnull:
                subprocess.check_call([path, "--version"], stdout=fnull)
                return True
        except subprocess.CalledProcessError:
            return False
