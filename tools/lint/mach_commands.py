# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import json
import logging
import os
import platform
import subprocess
import sys
import which
from distutils.version import LooseVersion

from mozbuild.base import (
    MachCommandBase,
)


from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
    SubCommand,
)


here = os.path.abspath(os.path.dirname(__file__))


ESLINT_PACKAGES = [
    "eslint@2.9.0",
    "eslint-plugin-html@1.4.0",
    "eslint-plugin-mozilla@0.0.3",
    "eslint-plugin-react@4.2.3"
]

ESLINT_NOT_FOUND_MESSAGE = '''
Could not find eslint!  We looked at the --binary option, at the ESLINT
environment variable, and then at your local node_modules path. Please Install
eslint and needed plugins with:

mach eslint --setup

and try again.
'''.strip()

NODE_NOT_FOUND_MESSAGE = '''
nodejs v4.2.3 is either not installed or is installed to a non-standard path.
Please install nodejs from https://nodejs.org and try again.

Valid installation paths:
'''.strip()

NPM_NOT_FOUND_MESSAGE = '''
Node Package Manager (npm) is either not installed or installed to a
non-standard path. Please install npm from https://nodejs.org (it comes as an
option in the node installation) and try again.

Valid installation paths:
'''.strip()


@CommandProvider
class MachCommands(MachCommandBase):

    @Command(
        'lint', category='devenv',
        description='Run linters.')
    @CommandArgument(
        'paths', nargs='*', default=None,
        help="Paths to file or directories to lint, like "
             "'browser/components/loop' or 'mobile/android'. "
             "Defaults to the current directory if not given.")
    @CommandArgument(
        '-l', '--linter', dest='linters', default=None, action='append',
        help="Linters to run, e.g 'eslint'. By default all linters are run "
             "for all the appropriate files.")
    @CommandArgument(
        '-f', '--format', dest='fmt', default='stylish',
        help="Formatter to use. Defaults to 'stylish'.")
    def lint(self, paths, linters=None, fmt='stylish', **lintargs):
        """Run linters."""
        from mozlint import LintRoller, formatters

        paths = paths or ['.']

        lint_files = self.find_linters(linters)

        lintargs['exclude'] = ['obj*']
        lint = LintRoller(**lintargs)
        lint.read(lint_files)

        # run all linters
        results = lint.roll(paths)

        status = 0
        if results:
            status = 1

        formatter = formatters.get(fmt)
        print(formatter(results))
        return status

    @SubCommand('lint', 'setup',
                "Setup required libraries for specified lints.")
    @CommandArgument(
        '-l', '--linter', dest='linters', default=None, action='append',
        help="Linters to run, e.g 'eslint'. By default all linters are run "
             "for all the appropriate files.")
    def lint_setup(self, linters=None, **lintargs):
        from mozlint import LintRoller

        lint_files = self.find_linters(linters)
        lint = LintRoller(lintargs=lintargs)
        lint.read(lint_files)

        for l in lint.linters:
            if 'setup' in l:
                l['setup']()

    def find_linters(self, linters=None):
        lints = []
        files = os.listdir(here)
        for f in files:
            name, ext = os.path.splitext(f)
            if ext != '.lint':
                continue

            if linters and name not in linters:
                continue

            lints.append(os.path.join(here, f))
        return lints

    @Command('eslint', category='devenv',
             description='Run eslint or help configure eslint for optimal development.')
    @CommandArgument('-s', '--setup', default=False, action='store_true',
                     help='configure eslint for optimal development.')
    @CommandArgument('-e', '--ext', default='[.js,.jsm,.jsx,.xml,.html]',
                     help='Filename extensions to lint, default: "[.js,.jsm,.jsx,.xml,.html]".')
    @CommandArgument('-b', '--binary', default=None,
                     help='Path to eslint binary.')
    @CommandArgument('args', nargs=argparse.REMAINDER)  # Passed through to eslint.
    def eslint(self, setup, ext=None, binary=None, args=None):
        '''Run eslint.'''

        module_path = self.get_eslint_module_path()

        # eslint requires at least node 4.2.3
        nodePath = self.getNodeOrNpmPath("node", LooseVersion("4.2.3"))
        if not nodePath:
            return 1

        if setup:
            return self.eslint_setup()

        npmPath = self.getNodeOrNpmPath("npm")
        if not npmPath:
            return 1

        if self.eslintModuleHasIssues():
            install = self._prompt_yn("\nContinuing will automatically fix "
                                      "these issues. Would you like to "
                                      "continue")
            if install:
                self.eslint_setup()
            else:
                return 1

        # Valid binaries are:
        #  - Any provided by the binary argument.
        #  - Any pointed at by the ESLINT environmental variable.
        #  - Those provided by mach eslint --setup.
        #
        #  eslint --setup installs some mozilla specific plugins and installs
        #  all node modules locally. This is the preferred method of
        #  installation.

        if not binary:
            binary = os.environ.get('ESLINT', None)

            if not binary:
                binary = os.path.join(module_path, "node_modules", ".bin", "eslint")
                if not os.path.isfile(binary):
                    binary = None

        if not binary:
            print(ESLINT_NOT_FOUND_MESSAGE)
            return 1

        self.log(logging.INFO, 'eslint', {'binary': binary, 'args': args},
                 'Running {binary}')

        args = args or ['.']

        cmd_args = [binary,
                    # Enable the HTML plugin.
                    # We can't currently enable this in the global config file
                    # because it has bad interactions with the SublimeText
                    # ESLint plugin (bug 1229874).
                    '--plugin', 'html',
                    '--ext', ext,  # This keeps ext as a single argument.
                    ] + args

        success = self.run_process(
            cmd_args,
            pass_thru=True,  # Allow user to run eslint interactively.
            ensure_exit_code=False,  # Don't throw on non-zero exit code.
            require_unix_environment=True  # eslint is not a valid Win32 binary.
        )

        self.log(logging.INFO, 'eslint', {'msg': ('No errors' if success == 0 else 'Errors')},
                 'Finished eslint. {msg} encountered.')
        return success

    def eslint_setup(self, update_only=False):
        """Ensure eslint is optimally configured.

        This command will inspect your eslint configuration and
        guide you through an interactive wizard helping you configure
        eslint for optimal use on Mozilla projects.
        """
        orig_cwd = os.getcwd()
        sys.path.append(os.path.dirname(__file__))

        module_path = self.get_eslint_module_path()

        # npm sometimes fails to respect cwd when it is run using check_call so
        # we manually switch folders here instead.
        os.chdir(module_path)

        npmPath = self.getNodeOrNpmPath("npm")
        if not npmPath:
            return 1

        # Install eslint and necessary plugins.
        for pkg in ESLINT_PACKAGES:
            name, version = pkg.split("@")
            success = False

            if self.node_package_installed(pkg, cwd=module_path):
                success = True
            else:
                if pkg.startswith("eslint-plugin-mozilla"):
                    cmd = [npmPath, "install",
                           os.path.join(module_path, "eslint-plugin-mozilla")]
                else:
                    cmd = [npmPath, "install", pkg]

                print("Installing %s v%s using \"%s\"..."
                      % (name, version, " ".join(cmd)))
                success = self.callProcess(pkg, cmd)

            if not success:
                return 1

        eslint_path = os.path.join(module_path, "node_modules", ".bin", "eslint")

        print("\nESLint and approved plugins installed successfully!")
        print("\nNOTE: Your local eslint binary is at %s\n" % eslint_path)

        os.chdir(orig_cwd)

    def callProcess(self, name, cmd, cwd=None):
        try:
            with open(os.devnull, "w") as fnull:
                subprocess.check_call(cmd, cwd=cwd, stdout=fnull)
        except subprocess.CalledProcessError:
            if cwd:
                print("\nError installing %s in the %s folder, aborting." % (name, cwd))
            else:
                print("\nError installing %s, aborting." % name)

            return False

        return True

    def eslintModuleHasIssues(self):
        has_issues = False
        node_module_path = os.path.join(self.get_eslint_module_path(), "node_modules")

        for pkg in ESLINT_PACKAGES:
            name, req_version = pkg.split("@")
            path = os.path.join(node_module_path, name, "package.json")

            if not os.path.exists(path):
                print("%s v%s needs to be installed locally." % (name, req_version))
                has_issues = True
                continue

            data = json.load(open(path))

            if data["version"] != req_version:
                print("%s v%s should be v%s." % (name, data["version"], req_version))
                has_issues = True

        return has_issues

    def node_package_installed(self, package_name="", globalInstall=False, cwd=None):
        try:
            npmPath = self.getNodeOrNpmPath("npm")

            cmd = [npmPath, "ls", "--parseable", package_name]

            if globalInstall:
                cmd.append("-g")

            with open(os.devnull, "w") as fnull:
                subprocess.check_call(cmd, stdout=fnull, stderr=fnull, cwd=cwd)

            return True
        except subprocess.CalledProcessError:
            return False

    def getPossibleNodePathsWin(self):
        """
        Return possible nodejs paths on Windows.
        """
        if platform.system() != "Windows":
            return []

        return list({
            "%s\\nodejs" % os.environ.get("SystemDrive"),
            os.path.join(os.environ.get("ProgramFiles"), "nodejs"),
            os.path.join(os.environ.get("PROGRAMW6432"), "nodejs"),
            os.path.join(os.environ.get("PROGRAMFILES"), "nodejs")
        })

    def getNodeOrNpmPath(self, filename, minversion=None):
        """
        Return the nodejs or npm path.
        """
        if platform.system() == "Windows":
            for ext in [".cmd", ".exe", ""]:
                try:
                    nodeOrNpmPath = which.which(filename + ext,
                                                path=self.getPossibleNodePathsWin())
                    if self.is_valid(nodeOrNpmPath, minversion):
                        return nodeOrNpmPath
                except which.WhichError:
                    pass
        else:
            try:
                nodeOrNpmPath = which.which(filename)
                if self.is_valid(nodeOrNpmPath, minversion):
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
                print("  - %s" % p)
        elif platform.system() == "Darwin":
            print("  - /usr/local/bin/node")
        elif platform.system() == "Linux":
            print("  - /usr/bin/nodejs")

        return None

    def is_valid(self, path, minversion=None):
        try:
            version_str = subprocess.check_output([path, "--version"],
                                                  stderr=subprocess.STDOUT)
            if minversion:
                # nodejs prefixes its version strings with "v"
                version = LooseVersion(version_str.lstrip('v'))
                return version >= minversion
            return True
        except (subprocess.CalledProcessError, OSError):
            return False

    def get_project_root(self):
        fullpath = os.path.abspath(sys.modules['__main__'].__file__)
        return os.path.dirname(fullpath)

    def get_eslint_module_path(self):
        return os.path.join(self.get_project_root(), "testing", "eslint")

    def _prompt_yn(self, msg):
        if not sys.stdin.isatty():
            return False

        print('%s? [Y/n]' % msg)

        while True:
            choice = raw_input().lower().strip()

            if not choice:
                return True

            if choice in ('y', 'yes'):
                return True

            if choice in ('n', 'no'):
                return False

            print('Must reply with one of {yes, no, y, n}.')
