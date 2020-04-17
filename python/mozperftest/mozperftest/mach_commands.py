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
    def run_perftest(
        self, flavor="script", test_objects=None, resolve_tests=True, **kwargs
    ):
        from mozperftest import MachEnvironment, Metadata

        kwargs["test_objects"] = test_objects
        kwargs["resolve_tests"] = resolve_tests
        env = MachEnvironment(self, flavor, **kwargs)
        metadata = Metadata(self, env, flavor)
        env.run_hook("before_runs")
        try:
            with env.frozen() as e:
                e.run(metadata)
        finally:
            env.run_hook("after_runs")
