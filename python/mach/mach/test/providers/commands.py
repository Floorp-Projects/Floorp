# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

from functools import partial

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)


def is_foo(cls):
    """Foo must be true"""
    return cls.foo


def is_bar(val, cls):
    """Bar must equal val"""
    return cls.bar == val


@CommandProvider
class MachCommands(object):
    foo = True
    bar = False

    @Command('cmd_foo', category='testing')
    @CommandArgument(
        '--arg', default=None,
        help="Argument help.")
    def run_foo(self):
        pass

    @Command('cmd_bar', category='testing',
             conditions=[partial(is_bar, False)])
    def run_bar(self):
        pass

    @Command('cmd_foobar', category='testing',
             conditions=[is_foo, partial(is_bar, True)])
    def run_foobar(self):
        pass
