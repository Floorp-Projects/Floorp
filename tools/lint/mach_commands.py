# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import json
import logging
import os
import platform
import re
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
)


here = os.path.abspath(os.path.dirname(__file__))


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

VERSION_RE = re.compile(r"^\d+\.\d+\.\d+$")
CARET_VERSION_RANGE_RE = re.compile(r"^\^((\d+)\.\d+\.\d+)$")


def setup_argument_parser():
    from mozlint import cli
    return cli.MozlintParser()


@CommandProvider
class MachCommands(MachCommandBase):

    @Command(
        'lint', category='devenv',
        description='Run linters.',
        parser=setup_argument_parser)
    def lint(self, *runargs, **lintargs):
        """Run linters."""
        from mozlint import cli
        lintargs['exclude'] = ['obj*']
        cli.SEARCH_PATHS.append(here)
        return cli.run(*runargs, **lintargs)

    @Command('eslint', category='devenv',
             description='Run eslint or help configure eslint for optimal development.')
    @CommandArgument('-s', '--setup', default=False, action='store_true',
                     help='Configure eslint for optimal development.')
    @CommandArgument('-e', '--ext', default='[.js,.jsm,.jsx,.xml,.html]',
                     help='Filename extensions to lint, default: "[.js,.jsm,.jsx,.xml,.html]".')
    @CommandArgument('-b', '--binary', default=None,
                     help='Path to eslint binary.')
    @CommandArgument('--fix', default=False, action='store_true',
                     help='Request that eslint automatically fix errors, where possible.')
    @CommandArgument('args', nargs=argparse.REMAINDER)  # Passed through to eslint.
    def eslint(self, setup, ext=None, binary=None, fix=False, args=None):
        '''Run eslint.'''

        module_path = self.get_eslint_module_path()

        # eslint requires at least node 4.2.3
        nodePath = self.get_node_or_npm_path("node", LooseVersion("4.2.3"))
        if not nodePath:
            return 1

        if setup:
            return self.eslint_setup()

        npm_path = self.get_node_or_npm_path("npm")
        if not npm_path:
            return 1

        if self.eslint_module_has_issues():
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

        # eslint requires that --fix be set before the --ext argument.
        if fix:
            cmd_args.insert(1, '--fix')

        success = self.run_process(
            cmd_args,
            pass_thru=True,  # Allow user to run eslint interactively.
            ensure_exit_code=False,  # Don't throw on non-zero exit code.
            require_unix_environment=True  # eslint is not a valid Win32 binary.
        )

        self.log(logging.INFO, 'eslint', {'msg': ('No errors' if success == 0 else 'Errors')},
                 'Finished eslint. {msg} encountered.')
        return success

    def eslint_setup(self):
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

        npm_path = self.get_node_or_npm_path("npm")
        if not npm_path:
            return 1

        # Install ESLint and external plugins
        cmd = [npm_path, "install"]
        print("Installing eslint for mach using \"%s\"..." % (" ".join(cmd)))
        if not self.call_process("eslint", cmd):
            return 1

        # Install in-tree ESLint plugin
        cmd = [npm_path, "install",
               os.path.join(module_path, "eslint-plugin-mozilla")]
        print("Installing eslint-plugin-mozilla using \"%s\"..." % (" ".join(cmd)))
        if not self.call_process("eslint-plugin-mozilla", cmd):
            return 1

        eslint_path = os.path.join(module_path, "node_modules", ".bin", "eslint")

        print("\nESLint and approved plugins installed successfully!")
        print("\nNOTE: Your local eslint binary is at %s\n" % eslint_path)

        os.chdir(orig_cwd)

    def call_process(self, name, cmd, cwd=None):
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

    def expected_eslint_modules(self):
        # Read the expected version of ESLint and external modules
        expected_modules_path = os.path.join(self.get_eslint_module_path(), "package.json")
        with open(expected_modules_path, "r") as f:
            expected_modules = json.load(f)["dependencies"]

        # Also read the in-tree ESLint plugin version
        mozilla_json_path = os.path.join(self.get_eslint_module_path(),
                                         "eslint-plugin-mozilla", "package.json")
        with open(mozilla_json_path, "r") as f:
            expected_modules["eslint-plugin-mozilla"] = json.load(f)["version"]

        return expected_modules

    def eslint_module_has_issues(self):
        has_issues = False
        node_modules_path = os.path.join(self.get_eslint_module_path(), "node_modules")

        for name, version_range in self.expected_eslint_modules().iteritems():
            path = os.path.join(node_modules_path, name, "package.json")

            if not os.path.exists(path):
                print("%s v%s needs to be installed locally." % (name, version_range))
                has_issues = True
                continue

            data = json.load(open(path))

            if not self.version_in_range(data["version"], version_range):
                print("%s v%s should be v%s." % (name, data["version"], version_range))
                has_issues = True

        return has_issues

    def version_in_range(self, version, version_range):
        """
        Check if a module version is inside a version range.  Only supports explicit versions and
        caret ranges for the moment, since that's all we've used so far.
        """
        if version == version_range:
            return True

        version_match = VERSION_RE.match(version)
        if not version_match:
            raise RuntimeError("mach eslint doesn't understand module version %s" % version)
        version = LooseVersion(version)

        # Caret ranges as specified by npm allow changes that do not modify the left-most non-zero
        # digit in the [major, minor, patch] tuple.  The code below assumes the major digit is
        # non-zero.
        range_match = CARET_VERSION_RANGE_RE.match(version_range)
        if range_match:
            range_version = range_match.group(1)
            range_major = int(range_match.group(2))

            range_min = LooseVersion(range_version)
            range_max = LooseVersion("%d.0.0" % (range_major + 1))

            return range_min <= version < range_max

        return False

    def get_possible_node_paths_win(self):
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

    def get_node_or_npm_path(self, filename, minversion=None):
        """
        Return the nodejs or npm path.
        """
        if platform.system() == "Windows":
            for ext in [".cmd", ".exe", ""]:
                try:
                    node_or_npm_path = which.which(filename + ext,
                                                   path=self.get_possible_node_paths_win())
                    if self.is_valid(node_or_npm_path, minversion):
                        return node_or_npm_path
                except which.WhichError:
                    pass
        else:
            try:
                node_or_npm_path = which.which(filename)
                if self.is_valid(node_or_npm_path, minversion):
                    return node_or_npm_path
            except which.WhichError:
                pass

        if filename == "node":
            print(NODE_NOT_FOUND_MESSAGE)
        elif filename == "npm":
            print(NPM_NOT_FOUND_MESSAGE)

        if platform.system() == "Windows":
            appPaths = self.get_possible_node_paths_win()

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
        return os.path.join(self.get_project_root(), "tools", "lint", "eslint")

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
