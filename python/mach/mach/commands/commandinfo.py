# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
from itertools import chain

from mach.decorators import (
    CommandProvider,
    Command,
    CommandArgument,
)
from mozbuild.base import MachCommandBase
from mozbuild.util import memoize, memoized_property


@CommandProvider
class BuiltinCommands(MachCommandBase):

    @property
    def command_handlers(self):
        """A dictionary of command handlers keyed by command name."""
        return self._mach_context.commands.command_handlers

    @memoized_property
    def commands(self):
        """A sorted list of all command names."""
        return sorted(self.command_handlers)

    @memoize
    def get_handler_options(self, handler):
        """Given a command handler, return all available options."""
        targets = []

        # The 'option_strings' are of the form [('-f', '--foo'), ('-b', '--bar'), ...].
        option_strings = [item[0] for item in handler.arguments]
        # Filter out positional arguments (we don't want to complete their metavar).
        option_strings = [opt for opt in option_strings if opt[0].startswith('-')]
        targets.extend(chain(*option_strings))

        # If the command uses its own ArgumentParser, extract options from there as well.
        if handler.parser:
            targets.extend(chain(*[action.option_strings
                                   for action in handler.parser._actions]))

        return targets

    @Command('mach-commands', category='misc',
             description='List all mach commands.')
    def run_commands(self):
        print("\n".join(self.commands))

    @Command('mach-debug-commands', category='misc',
             description='Show info about available mach commands.')
    @CommandArgument('match', metavar='MATCH', default=None, nargs='?',
                     help='Only display commands containing given substring.')
    def run_debug_commands(self, match=None):
        import inspect

        for command, handler in self.command_handlers.items():
            if match and match not in command:
                continue

            cls = handler.cls
            method = getattr(cls, getattr(handler, 'method'))

            print(command)
            print('=' * len(command))
            print('')
            print('File: %s' % inspect.getsourcefile(method))
            print('Class: %s' % cls.__name__)
            print('Method: %s' % handler.method)
            print('')

    @Command('mach-completion', category='misc',
             description='Prints a list of completion strings for the specified command.')
    @CommandArgument('args', default=None, nargs=argparse.REMAINDER,
                     help="Command to complete.")
    def run_completion(self, args):
        if not args:
            print("\n".join(self.commands))
            return

        is_help = 'help' in args
        command = None
        for i, arg in enumerate(args):
            if arg in self.commands:
                command = arg
                args = args[i+1:]
                break

        # If no command is typed yet, just offer the commands.
        if not command:
            print("\n".join(self.commands))
            return

        handler = self.command_handlers[command]
        # If a subcommand was typed, update the handler.
        for arg in args:
            if arg in handler.subcommand_handlers:
                handler = handler.subcommand_handlers[arg]
                break

        targets = sorted(handler.subcommand_handlers.keys())
        if is_help:
            print("\n".join(targets))
            return

        targets.append('help')
        targets.extend(self.get_handler_options(handler))
        print("\n".join(targets))
