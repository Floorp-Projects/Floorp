# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

from collections import namedtuple

# Holds mach run-time state so it can easily be passed to command providers.
CommandContext = namedtuple('CommandContext', ['topdir', 'cwd',
    'settings', 'log_manager', 'commands'])


class MethodHandler(object):
    """Describes a Python method that implements a mach command.

    Instances of these are produced by mach when it processes classes
    defining mach commands.
    """
    __slots__ = (
        # The Python class providing the command. This is the class type not
        # an instance of the class. Mach will instantiate a new instance of
        # the class if the command is executed.
        'cls',

        # Whether the __init__ method of the class should receive a mach
        # context instance. This should only affect the mach driver and how
        # it instantiates classes.
        'pass_context',

        # The name of the method providing the command. In other words, this
        # is the str name of the attribute on the class type corresponding to
        # the name of the function.
        'method',

        # The argparse subparser for this command's arguments.
        'parser',

        # Arguments passed to add_parser() on the main mach subparser. This is
        # a 2-tuple of positional and named arguments, respectively.
        'parser_args',

        # Arguments added to this command's parser. This is a 2-tuple of
        # positional and named arguments, respectively.
        'arguments',
    )

    def __init__(self, cls, method, parser_args, arguments=None,
        pass_context=False):

        self.cls = cls
        self.method = method
        self.parser_args = parser_args
        self.arguments = arguments or []
        self.pass_context = pass_context
