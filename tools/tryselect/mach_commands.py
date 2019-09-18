# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import importlib
import os
import sys

from mach.decorators import (
    CommandProvider,
    Command,
    SettingsProvider,
    SubCommand,
)
from mozboot.util import get_state_dir
from mozbuild.base import BuildEnvironmentNotFoundException, MachCommandBase


CONFIG_ENVIRONMENT_NOT_FOUND = '''
No config environment detected. This means we are unable to properly
detect test files in the specified paths or tags. Please run:

    $ mach configure

and try again.
'''.lstrip()


class get_parser(object):
    def __init__(self, selector):
        self.selector = selector

    def __call__(self):
        mod = importlib.import_module('tryselect.selectors.{}'.format(self.selector))
        return getattr(mod, '{}Parser'.format(self.selector.capitalize()))()


def generic_parser():
    from tryselect.cli import BaseTryParser
    parser = BaseTryParser()
    parser.add_argument('argv', nargs=argparse.REMAINDER)
    return parser


@SettingsProvider
class TryConfig(object):

    @classmethod
    def config_settings(cls):
        from mach.registrar import Registrar

        desc = "The default selector to use when running `mach try` without a subcommand."
        choices = Registrar.command_handlers['try'].subcommand_handlers.keys()

        return [
            ('try.default', 'string', desc, 'syntax', {'choices': choices}),
            ('try.maxhistory', 'int', "Maximum number of pushes to save in history.", 10),
        ]


@CommandProvider
class TrySelect(MachCommandBase):

    def __init__(self, context):
        super(TrySelect, self).__init__(context)
        from tryselect import push
        push.MAX_HISTORY = self._mach_context.settings['try']['maxhistory']
        self.subcommand = self._mach_context.handler.subcommand
        self.parser = self._mach_context.handler.parser
        self._presets = None

    @property
    def presets(self):
        if self._presets:
            return self._presets

        from tryselect.preset import MergedHandler

        # Create our handler using both local and in-tree presets. The first
        # path in this list will be treated as the 'user' file for the purposes
        # of saving and editing. All subsequent paths are 'read-only'. We check
        # an environment variable first for testing purposes.
        if os.environ.get('MACH_TRY_PRESET_PATHS'):
            preset_paths = os.environ['MACH_TRY_PRESET_PATHS'].split(os.pathsep)
        else:
            preset_paths = [
                os.path.join(get_state_dir(), 'try_presets.yml'),
                os.path.join(self.topsrcdir, 'tools', 'tryselect', 'try_presets.yml'),
            ]

        self._presets = MergedHandler(*preset_paths)
        return self._presets

    def handle_presets(self, preset_action, save, preset, **kwargs):
        """Handle preset related arguments.

        This logic lives here so that the underlying selectors don't need
        special preset handling. They can all save and load presets the same
        way.
        """
        from tryselect.preset import migrate_old_presets
        from tryselect.util.dicttools import merge

        user_presets = self.presets.handlers[0]

        # TODO: Remove after Jan 1, 2020.
        migrate_old_presets(user_presets)

        if preset_action == 'list':
            self.presets.list()
            sys.exit()

        if preset_action == 'edit':
            user_presets.edit()
            sys.exit()

        default = self.parser.get_default
        if save:
            selector = self.subcommand or self._mach_context.settings['try']['default']

            # Only save non-default values for simplicity.
            kwargs = {k: v for k, v in kwargs.items() if v != default(k)}
            user_presets.save(save, selector=selector, **kwargs)
            print('preset saved, run with: --preset={}'.format(save))
            sys.exit()

        if preset:
            if preset not in self.presets:
                self.parser.error("preset '{}' does not exist".format(preset))

            name = preset
            preset = self.presets[name]
            selector = preset.pop('selector')
            preset.pop('description', None)  # description isn't used by any selectors

            if not self.subcommand:
                self.subcommand = selector
            elif self.subcommand != selector:
                print("error: preset '{}' exists for a different selector "
                      "(did you mean to run 'mach try {}' instead?)".format(
                        name, selector))
                sys.exit(1)

            # Order of precedence is defaults -> presets -> cli. Configuration
            # from the right overwrites configuration from the left.
            defaults = {}
            nondefaults = {}
            for k, v in kwargs.items():
                if v == default(k):
                    defaults[k] = v
                else:
                    nondefaults[k] = v

            kwargs = merge(defaults, preset, nondefaults)

        return kwargs

    def handle_try_config(self, **kwargs):
        from tryselect.util.dicttools import merge
        kwargs.setdefault('try_config', {})
        for cls in self.parser.templates.itervalues():
            try_config = cls.try_config(**kwargs)
            if try_config is not None:
                kwargs['try_config'] = merge(kwargs['try_config'], try_config)

            for name in cls.dests:
                del kwargs[name]

        return kwargs

    def run(self, **kwargs):
        if 'preset' in self.parser.common_groups:
            kwargs = self.handle_presets(**kwargs)

        if self.parser.templates:
            kwargs = self.handle_try_config(**kwargs)

        mod = importlib.import_module('tryselect.selectors.{}'.format(self.subcommand))
        return mod.run(**kwargs)

    @Command('try',
             category='ci',
             description='Push selected tasks to the try server',
             parser=generic_parser)
    def try_default(self, argv=None, **kwargs):
        """Push selected tests to the try server.

        The |mach try| command is a frontend for scheduling tasks to
        run on try server using selectors. A selector is a subcommand
        that provides its own set of command line arguments and are
        listed below.

        If no subcommand is specified, the `syntax` selector is run by
        default. Run |mach try syntax --help| for more information on
        scheduling with the `syntax` selector.
        """
        # We do special handling of presets here so that `./mach try --preset foo`
        # works no matter what subcommand 'foo' was saved with.
        preset = kwargs['preset']
        if preset:
            if preset not in self.presets:
                self.parser.error("preset '{}' does not exist".format(preset))

            self.subcommand = self.presets[preset]['selector']

        sub = self.subcommand or self._mach_context.settings['try']['default']
        return self._mach_context.commands.dispatch(
            'try', subcommand=sub, context=self._mach_context, argv=argv, **kwargs)

    @SubCommand('try',
                'fuzzy',
                description='Select tasks on try using a fuzzy finder',
                parser=get_parser('fuzzy'))
    def try_fuzzy(self, **kwargs):
        """Select which tasks to run with a fuzzy finding interface (fzf).

        When entering the fzf interface you'll be confronted by two panes. The
        one on the left contains every possible task you can schedule, the one
        on the right contains the list of selected tasks. In other words, the
        tasks that will be scheduled once you you press <enter>.

        At first fzf will automatically select whichever task is under your
        cursor, which simplifies the case when you are looking for a single
        task. But normally you'll want to select many tasks. To accomplish
        you'll generally start by typing a query in the search bar to filter
        down the list of tasks (see Extended Search below). Then you'll either:

        A) Move the cursor to each task you want and press <tab> to select it.
        Notice it now shows up in the pane to the right.

        OR

        B) Press <ctrl-a> to select every task that matches your filter.

        You can delete your query, type a new one and select further tasks as
        many times as you like. Once you are happy with your selection, press
        <enter> to push the selected tasks to try.

        All selected task labels and their dependencies will be scheduled. This
        means you can select a test task and its build will automatically be
        filled in.


        Keyboard Shortcuts
        ------------------

        When in the fuzzy finder interface, start typing to filter down the
        task list. Then use the following keyboard shortcuts to select tasks:

          Ctrl-K / Up    => Move cursor up
          Ctrl-J / Down  => Move cursor down
          Tab            => Select task + move cursor down
          Shift-Tab      => Select task + move cursor up
          Ctrl-A         => Select all currently filtered tasks
          Ctrl-D         => De-select all currently filtered tasks
          Ctrl-T         => Toggle select all currently filtered tasks
          Alt-Bspace     => Clear query from input bar
          Enter          => Accept selection and exit
          Ctrl-C / Esc   => Cancel selection and exit
          ?              => Toggle preview pane

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


        Documentation
        -------------

        For more detailed documentation, please see:
        https://firefox-source-docs.mozilla.org/tools/try/selectors/fuzzy.html
        """
        if kwargs.pop('interactive'):
            kwargs['query'].append('INTERACTIVE')

        if kwargs.pop('intersection'):
            kwargs['intersect_query'] = kwargs['query']
            del kwargs['query']

        if kwargs.get('save') and not kwargs.get('query'):
            # If saving preset without -q/--query, allow user to use the
            # interface to build the query.
            kwargs_copy = kwargs.copy()
            kwargs_copy['push'] = False
            kwargs_copy['save'] = None
            kwargs['query'] = self.run(save_query=True, **kwargs_copy)

        if kwargs.get('paths'):
            kwargs['test_paths'] = kwargs['paths']

        return self.run(**kwargs)

    @SubCommand('try',
                'chooser',
                description='Schedule tasks by selecting them from a web '
                            'interface.',
                parser=get_parser('chooser'))
    def try_chooser(self, **kwargs):
        """Push tasks selected from a web interface to try.

        This selector will build the taskgraph and spin up a dynamically
        created 'trychooser-like' web-page on the localhost. After a selection
        has been made, pressing the 'Push' button will automatically push the
        selection to try.
        """
        self._activate_virtualenv()
        path = os.path.join('tools', 'tryselect', 'selectors', 'chooser', 'requirements.txt')
        self.virtualenv_manager.install_pip_requirements(path, quiet=True)

        return self.run(**kwargs)

    @SubCommand('try',
                'again',
                description='Schedule a previously generated (non try syntax) '
                            'push again.',
                parser=get_parser('again'))
    def try_again(self, **kwargs):
        return self.run(**kwargs)

    @SubCommand('try',
                'empty',
                description='Push to try without scheduling any tasks.',
                parser=get_parser('empty'))
    def try_empty(self, **kwargs):
        """Push to try, running no builds or tests

        This selector does not prompt you to run anything, it just pushes
        your patches to try, running no builds or tests by default. After
        the push finishes, you can manually add desired jobs to your push
        via Treeherder's Add New Jobs feature, located in the per-push
        menu.
        """
        return self.run(**kwargs)

    @SubCommand('try',
                'syntax',
                description='Select tasks on try using try syntax',
                parser=get_parser('syntax'))
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
        installable from mach vcs-setup) or a git repo using git-cinnabar
        (installable from mach vcs-setup).

        """
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

        return self.run(**kwargs)

    @SubCommand('try',
                'coverage',
                description='Select tasks on try using coverage data',
                parser=get_parser('coverage'))
    def try_coverage(self, **kwargs):
        """Select which tasks to use using coverage data.
        """
        return self.run(**kwargs)

    @SubCommand('try',
                'release',
                description='Push the current tree to try, configured for a staging release.',
                parser=get_parser('release'))
    def try_release(self, **kwargs):
        """Push the current tree to try, configured for a staging release.
        """
        return self.run(**kwargs)
