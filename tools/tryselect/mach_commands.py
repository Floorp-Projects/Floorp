# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import os
import sys

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
    SubCommand,
)

from mozbuild.base import BuildEnvironmentNotFoundException, MachCommandBase

CONFIG_ENVIRONMENT_NOT_FOUND = '''
No config environment detected. This means we are unable to properly
detect test files in the specified paths or tags. Please run:

    $ mach configure

and try again.
'''.lstrip()


def syntax_parser():
    from tryselect.selectors.syntax import arg_parser
    parser = arg_parser()
    # The --no-artifact flag is only interpreted locally by |mach try|; it's not
    # like the --artifact flag, which is interpreted remotely by the try server.
    #
    # We need a tri-state where set is different than the default value, so we
    # use a different variable than --artifact.
    parser.add_argument('--no-artifact',
                        dest='no_artifact',
                        action='store_true',
                        help='Force compiled (non-artifact) builds even when '
                             '--enable-artifact-builds is set.')
    return parser


@CommandProvider
class TrySelect(MachCommandBase):

    @Command('try',
             category='ci',
             description='Push selected tasks to the try server')
    @CommandArgument('args', nargs=argparse.REMAINDER)
    def try_default(self, args):
        """Push selected tests to the try server.

        The |mach try| command is a frontend for scheduling tasks to
        run on try server using selectors. A selector is a subcommand
        that provides its own set of command line arguments and are
        listed below.

        If no subcommand is specified, the `syntax` selector is run by
        default. Run |mach try syntax --help| for more information on
        scheduling with the `syntax` selector.
        """
        parser = syntax_parser()
        kwargs = vars(parser.parse_args(args))
        return self._mach_context.commands.dispatch(
            'try', subcommand='syntax', context=self._mach_context, **kwargs)

    @SubCommand('try',
                'fuzzy',
                description='Select tasks on try using a fuzzy finder')
    @CommandArgument('-u', '--update', action='store_true', default=False,
                     help="Update fzf before running")
    def try_fuzzy(self, update):
        """Select which tasks to use with fzf.

        This selector runs all task labels through a fuzzy finding interface.
        All selected task labels and their dependencies will be scheduled on
        try.

        Keyboard Shortcuts
        ------------------

        When in the fuzzy finder interface, start typing to filter down the
        task list. Then use the following keyboard shortcuts to select tasks:

          accept: <enter>
          cancel: <ctrl-c> or <esc>
          cursor-up: <ctrl-k> or <up>
          cursor-down: <ctrl-j> or <down>
          toggle-select-down: <tab>
          toggle-select-up: <shift-tab>
          select-all: <ctrl-a>
          deselect-all: <ctrl-d>
          toggle-all: <ctrl-t>
          clear-input: <alt-bspace>

        There are many more shortcuts enabled by default, you can also define
        your own shortcuts by setting `--bind` in the $FZF_DEFAULT_OPTS
        environment variable. See `man fzf` for more info.

        Extended Search
        ---------------

        When typing in search terms, the following modifiers can be applied:

          'word: exact match (line must contain the literal string "word")
          ^word: exact prefix match (line must start with literal "word")
          word$: exact suffix match (line must end with literal "word")
          !word: exact negation match (line must not contain literal "word")
          'a | 'b: OR operator (joins two exact match operators together)

        For example:

          ^start 'exact | !ignore fuzzy end$
        """
        from tryselect.selectors.fuzzy import run_fuzzy_try
        return run_fuzzy_try(update)

    @SubCommand('try',
                'syntax',
                description='Select tasks on try using try syntax',
                parser=syntax_parser)
    def try_syntax(self, **kwargs):
        """Push the current tree to try, with the specified syntax.

        Build options, platforms and regression tests may be selected
        using the usual try options (-b, -p and -u respectively). In
        addition, tests in a given directory may be automatically
        selected by passing that directory as a positional argument to the
        command. For example:

        mach try -b d -p linux64 dom testing/web-platform/tests/dom

        would schedule a try run for linux64 debug consisting of all
        tests under dom/ and testing/web-platform/tests/dom.

        Test selection using positional arguments is available for
        mochitests, reftests, xpcshell tests and web-platform-tests.

        Tests may be also filtered by passing --tag to the command,
        which will run only tests marked as having the specified
        tags e.g.

        mach try -b d -p win64 --tag media

        would run all tests tagged 'media' on Windows 64.

        If both positional arguments or tags and -u are supplied, the
        suites in -u will be run in full. Where tests are selected by
        positional argument they will be run in a single chunk.

        If no build option is selected, both debug and opt will be
        scheduled. If no platform is selected a default is taken from
        the AUTOTRY_PLATFORM_HINT environment variable, if set.

        The command requires either its own mercurial extension ("push-to-try",
        installable from mach mercurial-setup) or a git repo using git-cinnabar
        (available at https://github.com/glandium/git-cinnabar).

        """
        from mozbuild.testing import TestResolver
        from tryselect.selectors.syntax import AutoTry

        try:
            if self.substs.get("MOZ_ARTIFACT_BUILDS"):
                kwargs['local_artifact_build'] = True
        except BuildEnvironmentNotFoundException:
            # If we don't have a build locally, we can't tell whether
            # an artifact build is desired, but we still want the
            # command to succeed, if possible.
            pass

        config_status = os.path.join(self.topobjdir, 'config.status')
        if (kwargs['paths'] or kwargs['tags']) and not config_status:
            print(CONFIG_ENVIRONMENT_NOT_FOUND)
            sys.exit(1)

        def resolver_func():
            return self._spawn(TestResolver)

        at = AutoTry(self.topsrcdir, resolver_func, self._mach_context)
        return at.run(**kwargs)
