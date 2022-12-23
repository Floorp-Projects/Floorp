# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from mach.decorators import Command, CommandArgument


@Command("cmd_foo", category="testing")
def run_foo(command_context):
    pass


@Command("cmd_bar", category="testing")
@CommandArgument("--baz", action="store_true", help="Run with baz")
def run_bar(command_context, baz=None):
    pass
