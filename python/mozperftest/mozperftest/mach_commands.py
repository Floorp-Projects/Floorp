# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
import random
from functools import partial
from mach.decorators import CommandProvider, Command
from mozbuild.base import MachCommandBase, MachCommandConditions as conditions


def get_perftest_parser():
    from mozperftest import PerftestArgumentParser

    return PerftestArgumentParser


@CommandProvider
class Perftest(MachCommandBase):
    def _build_test_list(self, tests, randomized=False):
        res = []
        for test in tests:
            if os.path.isfile(test):
                tests.append(test)
            elif os.path.isdir(test):
                for root, dirs, files in os.walk(test):
                    for file in files:
                        if not file.startswith("perftest"):
                            continue
                        res.append(os.path.join(root, file))
        if not randomized:
            res.sort()
        else:
            # random shuffling is used to make sure
            # we don't always run tests in the same order
            random.shuffle(res)
        return res

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

        MachCommandBase._activate_virtualenv(self)
        kwargs["tests"] = self._build_test_list(
            kwargs["tests"], randomized=flavor != "doc"
        )

        if flavor == "doc":
            from mozperftest.utils import install_package

            location = os.path.join(self.topsrcdir, "third_party", "python", "esprima")
            install_package(self.virtualenv_manager, location)

            from mozperftest.scriptinfo import ScriptInfo

            for test in kwargs["tests"]:
                print(ScriptInfo(test))
            return

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
