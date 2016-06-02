# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os

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

        formatter = formatters.get(fmt)
        print(formatter(results))

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
