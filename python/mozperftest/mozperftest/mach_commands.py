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
        env.run_hook("before_cycles")
        cycles = env.get_arg("cycles", 1)
        metadata = Metadata(self, env, flavor)
        try:
            # XXX put the cycles inside the browser layer
            for cycle in range(1, cycles + 1):
                with env.frozen() as e:
                    e.run_hook("before_cycle", cycle=cycle)
                    try:
                        e.run(metadata)
                    finally:
                        e.run_hook("after_cycle", cycle=cycle)
        finally:
            env.run_hook("after_cycles")
