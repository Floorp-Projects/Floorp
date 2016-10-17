# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import os

from mozbuild.base import (
    MachCommandBase,
)


from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)


here = os.path.abspath(os.path.dirname(__file__))


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
    @CommandArgument('paths', default=None, nargs='*',
                     help="Paths to file or directories to lint, like "
                          "'browser/components/loop' Defaults to the "
                          "current directory if not given.")
    @CommandArgument('-s', '--setup', default=False, action='store_true',
                     help='Configure eslint for optimal development.')
    @CommandArgument('-b', '--binary', default=None,
                     help='Path to eslint binary.')
    @CommandArgument('--fix', default=False, action='store_true',
                     help='Request that eslint automatically fix errors, where possible.')
    @CommandArgument('extra_args', nargs=argparse.REMAINDER,
                     help='Extra args that will be forwarded to eslint.')
    def eslint(self, paths, extra_args=[], **kwargs):
        self._mach_context.commands.dispatch('lint', self._mach_context,
                                             linters=['eslint'], paths=paths,
                                             argv=extra_args, **kwargs)
