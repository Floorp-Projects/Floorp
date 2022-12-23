# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mach.decorators import Command


def is_true(cls):
    return True


def is_false(cls):
    return False


@Command("cmd_condition_true", category="testing", conditions=[is_true])
def run_condition_true(self, command_context):
    pass


@Command("cmd_condition_false", category="testing", conditions=[is_false])
def run_condition_false(self, command_context):
    pass


@Command(
    "cmd_condition_true_and_false", category="testing", conditions=[is_true, is_false]
)
def run_condition_true_and_false(self, command_context):
    pass


def is_ctx_foo(cls):
    """Foo must be true"""
    return cls._mach_context.foo


def is_ctx_bar(cls):
    """Bar must be true"""
    return cls._mach_context.bar


@Command("cmd_foo_ctx", category="testing", conditions=[is_ctx_foo])
def run_foo_ctx(self, command_context):
    pass


@Command("cmd_bar_ctx", category="testing", conditions=[is_ctx_bar])
def run_bar_ctx(self, command_context):
    pass


@Command("cmd_foobar_ctx", category="testing", conditions=[is_ctx_foo, is_ctx_bar])
def run_foobar_ctx(self, command_context):
    pass
