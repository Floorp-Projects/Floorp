# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
from __future__ import unicode_literals

from mach.decorators import (
    CommandProvider,
    Command,
)
from mozbuild.base import MachCommandBase


def is_foo(cls):
    """Foo must be true"""
    return cls.foo


def is_bar(cls):
    """Bar must be true"""
    return cls.bar


@CommandProvider
class ConditionsProvider(MachCommandBase):
    foo = True
    bar = False

    @Command("cmd_foo", category="testing", conditions=[is_foo])
    def run_foo(self, command_context):
        pass

    @Command("cmd_bar", category="testing", conditions=[is_bar])
    def run_bar(self, command_context):
        pass

    @Command("cmd_foobar", category="testing", conditions=[is_foo, is_bar])
    def run_foobar(self, command_context):
        pass


@CommandProvider
class ConditionsContextProvider(MachCommandBase):
    @Command("cmd_foo_ctx", category="testing", conditions=[is_foo])
    def run_foo(self, command_context):
        pass

    @Command("cmd_bar_ctx", category="testing", conditions=[is_bar])
    def run_bar(self, command_context):
        pass

    @Command("cmd_foobar_ctx", category="testing", conditions=[is_foo, is_bar])
    def run_foobar(self, command_context):
        pass
