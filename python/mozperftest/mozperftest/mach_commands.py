# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from functools import partial
from mach.decorators import CommandProvider, Command
from mozbuild.base import MachCommandBase, MachCommandConditions as conditions


def get_perftest_parser():
    from mozperftest import PerftestArgumentParser

    return PerftestArgumentParser


@CommandProvider
class Perftest(MachCommandBase):
    @Command(
        "perftest",
        category="testing",
        conditions=[partial(conditions.is_buildapp_in, apps=["firefox", "android"])],
        description="Run any flavor of perftest",
        parser=get_perftest_parser,
    )
    def run_perftest(self, **kwargs):
        MachCommandBase._activate_virtualenv(self)

        from mozperftest.runner import run_tests

        run_tests(mach_cmd=self, **kwargs)
