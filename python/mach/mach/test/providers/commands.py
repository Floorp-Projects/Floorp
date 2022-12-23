# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from functools import partial

from mach.decorators import Command, CommandArgument


def is_foo(cls):
    """Foo must be true"""
    return cls.foo


def is_bar(val, cls):
    """Bar must equal val"""
    return cls.bar == val


@Command("cmd_foo", category="testing")
@CommandArgument("--arg", default=None, help="Argument help.")
def run_foo(command_context):
    pass


@Command("cmd_bar", category="testing", conditions=[partial(is_bar, False)])
def run_bar(command_context):
    pass


@Command("cmd_foobar", category="testing", conditions=[is_foo, partial(is_bar, True)])
def run_foobar(command_context):
    pass
