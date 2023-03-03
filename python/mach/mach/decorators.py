# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import collections
import collections.abc
from typing import Optional

from mozbuild.base import MachCommandBase

from .base import MachError
from .registrar import Registrar


class _MachCommand(object):
    """Container for mach command metadata."""

    __slots__ = (
        # Content from decorator arguments to define the command.
        "name",
        "subcommand",
        "category",
        "description",
        "conditions",
        "_parser",
        "arguments",
        "argument_group_names",
        "virtualenv_name",
        "ok_if_tests_disabled",
        # By default, subcommands will be sorted. If this is set to
        # 'declaration', they will be left in declaration order.
        "order",
        # This is the function or callable that will be called when
        # the command is invoked
        "func",
        # The path to the `metrics.yaml` file that describes data that telemetry will
        # gather for this command. This path is optional.
        "metrics_path",
        # Dict of string to _MachCommand defining sub-commands for this
        # command.
        "subcommand_handlers",
        # For subcommands, the global order that the subcommand's declaration
        # was seen.
        "decl_order",
        # Whether to disable automatic logging to last_log.json for the command.
        "no_auto_log",
    )

    def __init__(
        self,
        name=None,
        subcommand=None,
        category=None,
        description=None,
        conditions=None,
        parser=None,
        order=None,
        virtualenv_name=None,
        ok_if_tests_disabled=False,
        no_auto_log=False,
    ):
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
        if ok_if_tests_disabled and category != "testing":
            raise ValueError(
                "ok_if_tests_disabled should only be set for " "`testing` mach commands"
            )
        self.ok_if_tests_disabled = ok_if_tests_disabled

        self.func = None
        self.metrics_path = None
        self.subcommand_handlers = {}
        self.decl_order = None
        self.no_auto_log = no_auto_log

    def create_instance(self, context, virtualenv_name):
        metrics = None
        if self.metrics_path:
            metrics = context.telemetry.metrics(self.metrics_path)

        # This ensures the resulting class is defined inside `mach` so that logging
        # works as expected, and has a meaningful name
        subclass = type(self.name, (MachCommandBase,), {})
        return subclass(
            context,
            virtualenv_name=virtualenv_name,
            metrics=metrics,
            no_auto_log=self.no_auto_log,
        )

    @property
    def parser(self):
        # Creating CLI parsers at command dispatch time can be expensive. Make
        # it possible to lazy load them by using functions.
        if callable(self._parser):
            self._parser = self._parser()

        return self._parser

    @property
    def docstring(self):
        return self.func.__doc__

    def __ior__(self, other):
        if not isinstance(other, _MachCommand):
            raise ValueError("can only operate on _MachCommand instances")

        for a in self.__slots__:
            if not getattr(self, a):
                setattr(self, a, getattr(other, a))

        return self

    def register(self, func):
        """Register the command in the Registrar with the function to be called on invocation."""
        if not self.subcommand:
            if not self.conditions and Registrar.require_conditions:
                return

            msg = (
                "Mach command '%s' implemented incorrectly. "
                + "Conditions argument must take a list "
                + "of functions. Found %s instead."
            )

            if not isinstance(self.conditions, collections.abc.Iterable):
                msg = msg % (self.name, type(self.conditions))
                raise MachError(msg)

            for c in self.conditions:
                if not hasattr(c, "__call__"):
                    msg = msg % (self.name, type(c))
                    raise MachError(msg)

            self.func = func

            Registrar.register_command_handler(self)

        else:
            if self.name not in Registrar.command_handlers:
                raise MachError(
                    "Command referenced by sub-command does not exist: %s" % self.name
                )

            self.func = func
            parent = Registrar.command_handlers[self.name]

            if self.subcommand in parent.subcommand_handlers:
                raise MachError("sub-command already defined: %s" % self.subcommand)

            parent.subcommand_handlers[self.subcommand] = self


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

    .. code-block:: python

        @Command('foo', category='misc', description='Run the foo action')
        def foo(self, command_context):
            pass
    """

    def __init__(self, name, metrics_path: Optional[str] = None, **kwargs):
        self._mach_command = _MachCommand(name=name, **kwargs)
        self._mach_command.metrics_path = metrics_path

    def __call__(self, func):
        if not hasattr(func, "_mach_command"):
            func._mach_command = _MachCommand()

        func._mach_command |= self._mach_command
        func._mach_command.register(func)

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

    def __init__(
        self,
        command,
        subcommand,
        description=None,
        parser=None,
        metrics_path: Optional[str] = None,
        virtualenv_name: Optional[str] = None,
    ):
        self._mach_command = _MachCommand(
            name=command,
            subcommand=subcommand,
            description=description,
            parser=parser,
            virtualenv_name=virtualenv_name,
        )
        self._mach_command.decl_order = SubCommand.global_order
        SubCommand.global_order += 1

        self._mach_command.metrics_path = metrics_path

    def __call__(self, func):
        if not hasattr(func, "_mach_command"):
            func._mach_command = _MachCommand()

        func._mach_command |= self._mach_command
        func._mach_command.register(func)

        return func


class CommandArgument(object):
    """Decorator for additional arguments to mach subcommands.

    This decorator should be used to add arguments to mach commands. Arguments
    to the decorator are proxied to ArgumentParser.add_argument().

    For example:

    .. code-block:: python

        @Command('foo', help='Run the foo action')
        @CommandArgument('-b', '--bar', action='store_true', default=False,
            help='Enable bar mode.')
        def foo(self, command_context):
            pass
    """

    def __init__(self, *args, **kwargs):
        if kwargs.get("nargs") == argparse.REMAINDER:
            # These are the assertions we make in dispatcher.py about
            # those types of CommandArguments.
            assert len(args) == 1
            assert all(
                k in ("default", "nargs", "help", "group", "metavar") for k in kwargs
            )
        self._command_args = (args, kwargs)

    def __call__(self, func):
        if not hasattr(func, "_mach_command"):
            func._mach_command = _MachCommand()

        func._mach_command.arguments.insert(0, self._command_args)

        return func


class CommandArgumentGroup(object):
    """Decorator for additional argument groups to mach commands.

    This decorator should be used to add arguments groups to mach commands.
    Arguments to the decorator are proxied to
    ArgumentParser.add_argument_group().

    For example:

    .. code-block: python

        @Command('foo', helps='Run the foo action')
        @CommandArgumentGroup('group1')
        @CommandArgument('-b', '--bar', group='group1', action='store_true',
            default=False, help='Enable bar mode.')
        def foo(self, command_context):
            pass

    The name should be chosen so that it makes sense as part of the phrase
    'Command Arguments for <name>' because that's how it will be shown in the
    help message.
    """

    def __init__(self, group_name):
        self._group_name = group_name

    def __call__(self, func):
        if not hasattr(func, "_mach_command"):
            func._mach_command = _MachCommand()

        func._mach_command.argument_group_names.insert(0, self._group_name)

        return func


def SettingsProvider(cls):
    """Class decorator to denote that this class provides Mach settings.

    When this decorator is encountered, the underlying class will automatically
    be registered with the Mach registrar and will (likely) be hooked up to the
    mach driver.
    """
    if not hasattr(cls, "config_settings"):
        raise MachError(
            "@SettingsProvider must contain a config_settings attribute. It "
            "may either be a list of tuples, or a callable that returns a list "
            "of tuples. Each tuple must be of the form:\n"
            "(<section>.<option>, <type_cls>, <description>, <default>, <choices>)\n"
            "as specified by ConfigSettings._format_metadata."
        )

    Registrar.register_settings_provider(cls)
    return cls
