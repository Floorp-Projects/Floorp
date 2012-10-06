# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import types

from mach.registrar import register_method_handler


def CommandProvider(cls):
    """Class decorator to denote that it provides subcommands for Mach.

    When this decorator is present, mach looks for commands being defined by
    methods inside the class.
    """

    # The implementation of this decorator relies on the parse-time behavior of
    # decorators. When the module is imported, the method decorators (like
    # @Command and @CommandArgument) are called *before* this class decorator.
    # The side-effect of the method decorators is to store specifically-named
    # attributes on the function types. We just scan over all functions in the
    # class looking for the side-effects of the method decorators.

    # We scan __dict__ because we only care about the classes own attributes,
    # not inherited ones. If we did inherited attributes, we could potentially
    # define commands multiple times. We also sort keys so commands defined in
    # the same class are grouped in a sane order.
    for attr in sorted(cls.__dict__.keys()):
        value = cls.__dict__[attr]

        if not isinstance(value, types.FunctionType):
            continue

        parser_args = getattr(value, '_mach_command', None)
        if parser_args is None:
            continue

        arguments = getattr(value, '_mach_command_args', None)

        register_method_handler(cls, attr, (parser_args[0], parser_args[1]),
            arguments or [])

    return cls


class Command(object):
    """Decorator for functions or methods that provide a mach subcommand.

    The decorator accepts arguments that would be passed to add_parser() of an
    ArgumentParser instance created via add_subparsers(). Essentially, it
    accepts the arguments one would pass to add_argument().

    For example:

        @Command('foo', help='Run the foo action')
        def foo(self):
            pass
    """
    def __init__(self, *args, **kwargs):
        self._command_args = (args, kwargs)

    def __call__(self, func):
        func._mach_command = self._command_args

        return func


class CommandArgument(object):
    """Decorator for additional arguments to mach subcommands.

    This decorator should be used to add arguments to mach commands. Arguments
    to the decorator are proxied to ArgumentParser.add_argument().

    For example:

        @Command('foo', help='Run the foo action')
        @CommandArgument('-b', '--bar', action='store_true', default=False,
            help='Enable bar mode.')
        def foo(self):
            pass
    """
    def __init__(self, *args, **kwargs):
        self._command_args = (args, kwargs)

    def __call__(self, func):
        command_args = getattr(func, '_mach_command_args', [])

        command_args.append(self._command_args)

        func._mach_command_args = command_args

        return func
