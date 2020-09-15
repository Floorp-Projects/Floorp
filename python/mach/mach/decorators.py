# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import argparse
import collections

from .base import MachError
from .registrar import Registrar
from mozbuild.base import MachCommandBase


class _MachCommand(object):
    """Container for mach command metadata."""

    __slots__ = (
        # Content from decorator arguments to define the command.
        'name',
        'subcommand',
        'category',
        'description',
        'conditions',
        '_parser',
        'arguments',
        'argument_group_names',
        'virtualenv_name',
        'ok_if_tests_disabled',

        # By default, subcommands will be sorted. If this is set to
        # 'declaration', they will be left in declaration order.
        'order',

        # Describes how dispatch is performed.

        # The Python class providing the command. This is the class type not
        # an instance of the class. Mach will instantiate a new instance of
        # the class if the command is executed.
        'cls',

        # The path to the `metrics.yaml` file that describes data that telemetry will
        # gather for this command. This path is optional.
        'metrics_path',

        # The name of the method providing the command. In other words, this
        # is the str name of the attribute on the class type corresponding to
        # the name of the function.
        'method',

        # Dict of string to _MachCommand defining sub-commands for this
        # command.
        'subcommand_handlers',

        # For subcommands, the global order that the subcommand's declaration
        # was seen.
        'decl_order',
    )

    def __init__(self, name=None, subcommand=None, category=None,
                 description=None, conditions=None, parser=None,
                 order=None, virtualenv_name=None, ok_if_tests_disabled=False):
        self.name = name
        self.subcommand = subcommand
        self.category = category
        self.description = description
        self.conditions = conditions or []
        self._parser = parser
        self.arguments = []
        self.argument_group_names = []
        self.virtualenv_name = virtualenv_name
        self.order = order
        if ok_if_tests_disabled and category != 'testing':
            raise ValueError('ok_if_tests_disabled should only be set for '
                             '`testing` mach commands')
        self.ok_if_tests_disabled = ok_if_tests_disabled

        self.cls = None
        self.metrics_path = None
        self.method = None
        self.subcommand_handlers = {}
        self.decl_order = None

    def create_instance(self, context, virtualenv_name):
        metrics = None
        if self.metrics_path:
            metrics = context.telemetry.metrics(self.metrics_path)
        return self.cls(context, virtualenv_name=virtualenv_name, metrics=metrics)

    @property
    def parser(self):
        # Creating CLI parsers at command dispatch time can be expensive. Make
        # it possible to lazy load them by using functions.
        if callable(self._parser):
            self._parser = self._parser()

        return self._parser

    @property
    def docstring(self):
        return self.cls.__dict__[self.method].__doc__

    def __ior__(self, other):
        if not isinstance(other, _MachCommand):
            raise ValueError('can only operate on _MachCommand instances')

        for a in self.__slots__:
            if not getattr(self, a):
                setattr(self, a, getattr(other, a))

        return self


def CommandProvider(_cls=None, metrics_path=None):
    def finalize(cls):
        if not issubclass(cls, MachCommandBase):
            raise MachError(
                'Mach command provider class %s must be a subclass of '
                'mozbuild.base.MachComandBase' % cls.__name__)

        seen_commands = set()

        # We scan __dict__ because we only care about the classes' own attributes,
        # not inherited ones. If we did inherited attributes, we could potentially
        # define commands multiple times. We also sort keys so commands defined in
        # the same class are grouped in a sane order.
        command_methods = sorted([
            (name, value._mach_command)
            for name, value in cls.__dict__.items()
            if hasattr(value, '_mach_command')
        ])

        for method, command in command_methods:
            # Ignore subcommands for now: we handle them later.
            if command.subcommand:
                continue

            seen_commands.add(command.name)

            if not command.conditions and Registrar.require_conditions:
                continue

            msg = 'Mach command \'%s\' implemented incorrectly. ' + \
                  'Conditions argument must take a list ' + \
                  'of functions. Found %s instead.'

            if not isinstance(command.conditions, collections.Iterable):
                msg = msg % (command.name, type(command.conditions))
                raise MachError(msg)

            for c in command.conditions:
                if not hasattr(c, '__call__'):
                    msg = msg % (command.name, type(c))
                    raise MachError(msg)

            command.cls = cls
            command.metrics_path = metrics_path
            command.method = method

            Registrar.register_command_handler(command)

        # Now do another pass to get sub-commands. We do this in two passes so
        # we can check the parent command existence without having to hold
        # state and reconcile after traversal.
        for method, command in command_methods:
            # It is a regular command.
            if not command.subcommand:
                continue

            if command.name not in seen_commands:
                raise MachError('Command referenced by sub-command does not '
                                'exist: %s' % command.name)

            if command.name not in Registrar.command_handlers:
                continue

            command.cls = cls
            command.metrics_path = metrics_path
            command.method = method
            parent = Registrar.command_handlers[command.name]

            if command.subcommand in parent.subcommand_handlers:
                raise MachError('sub-command already defined: %s' % command.subcommand)

            parent.subcommand_handlers[command.subcommand] = command

        return cls

    if _cls:
        # The CommandProvider was used without parameters, e.g.:
        #
        # @CommandProvider
        # class Example:
        #     ...
        # Invoke finalize() immediately
        return finalize(_cls)
    else:
        # The CommandProvider was used with parameters, e.g.:
        #
        # @CommandProvider(metrics_path='...')
        # class Example:
        #     ...
        # Return a callback which will be parameterized with the decorated class
        return finalize


class Command(object):
    """Decorator for functions or methods that provide a mach command.

    The decorator accepts arguments that define basic attributes of the
    command. The following arguments are recognized:

         category -- The string category to which this command belongs. Mach's
             help will group commands by category.

         description -- A brief description of what the command does.

         parser -- an optional argparse.ArgumentParser instance or callable
             that returns an argparse.ArgumentParser instance to use as the
             basis for the command arguments.

    For example:

        @Command('foo', category='misc', description='Run the foo action')
        def foo(self):
            pass
    """
    def __init__(self, name, **kwargs):
        self._mach_command = _MachCommand(name=name, **kwargs)

    def __call__(self, func):
        if not hasattr(func, '_mach_command'):
            func._mach_command = _MachCommand()

        func._mach_command |= self._mach_command

        return func


class SubCommand(object):
    """Decorator for functions or methods that provide a sub-command.

    Mach commands can have sub-commands. e.g. ``mach command foo`` or
    ``mach command bar``. Each sub-command has its own parser and is
    effectively its own mach command.

    The decorator accepts arguments that define basic attributes of the
    sub command:

        command -- The string of the command this sub command should be
        attached to.

        subcommand -- The string name of the sub command to register.

        description -- A textual description for this sub command.
    """

    global_order = 0

    def __init__(self, command, subcommand, description=None, parser=None):
        self._mach_command = _MachCommand(name=command, subcommand=subcommand,
                                          description=description,
                                          parser=parser)
        self._mach_command.decl_order = SubCommand.global_order
        SubCommand.global_order += 1

    def __call__(self, func):
        if not hasattr(func, '_mach_command'):
            func._mach_command = _MachCommand()

        func._mach_command |= self._mach_command

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
        if kwargs.get('nargs') == argparse.REMAINDER:
            # These are the assertions we make in dispatcher.py about
            # those types of CommandArguments.
            assert len(args) == 1
            assert all(k in ('default', 'nargs', 'help', 'group', 'metavar') for k in kwargs)
        self._command_args = (args, kwargs)

    def __call__(self, func):
        if not hasattr(func, '_mach_command'):
            func._mach_command = _MachCommand()

        func._mach_command.arguments.insert(0, self._command_args)

        return func


class CommandArgumentGroup(object):
    """Decorator for additional argument groups to mach commands.

    This decorator should be used to add arguments groups to mach commands.
    Arguments to the decorator are proxied to
    ArgumentParser.add_argument_group().

    For example:

        @Command('foo', helps='Run the foo action')
        @CommandArgumentGroup('group1')
        @CommandArgument('-b', '--bar', group='group1', action='store_true',
            default=False, help='Enable bar mode.')
        def foo(self):
            pass

    The name should be chosen so that it makes sense as part of the phrase
    'Command Arguments for <name>' because that's how it will be shown in the
    help message.
    """
    def __init__(self, group_name):
        self._group_name = group_name

    def __call__(self, func):
        if not hasattr(func, '_mach_command'):
            func._mach_command = _MachCommand()

        func._mach_command.argument_group_names.insert(0, self._group_name)

        return func


def SettingsProvider(cls):
    """Class decorator to denote that this class provides Mach settings.

    When this decorator is encountered, the underlying class will automatically
    be registered with the Mach registrar and will (likely) be hooked up to the
    mach driver.
    """
    if not hasattr(cls, 'config_settings'):
        raise MachError('@SettingsProvider must contain a config_settings attribute. It '
                        'may either be a list of tuples, or a callable that returns a list '
                        'of tuples. Each tuple must be of the form:\n'
                        '(<section>.<option>, <type_cls>, <description>, <default>, <choices>)\n'
                        'as specified by ConfigSettings._format_metadata.')

    Registrar.register_settings_provider(cls)
    return cls
