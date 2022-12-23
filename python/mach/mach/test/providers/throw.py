# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mach.decorators import Command, CommandArgument
from mach.test.providers import throw2


@Command("throw", category="testing")
@CommandArgument("--message", "-m", default="General Error")
def throw(command_context, message):
    raise Exception(message)


@Command("throw_deep", category="testing")
@CommandArgument("--message", "-m", default="General Error")
def throw_deep(command_context, message):
    throw2.throw_deep(message)
