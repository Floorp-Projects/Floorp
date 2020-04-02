# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from functools import partial

from mach.decorators import CommandProvider, Command
from mozbuild.base import MachCommandBase, MachCommandConditions as conditions


def get_perftest_parser():
    from mozperftest import get_parser

    return get_parser


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
        from mozperftest import get_env, get_metadata

        system, browser, metrics = get_env(self, flavor, test_objects, resolve_tests)
        metadata = get_metadata(self, flavor, **kwargs)

        with system, browser, metrics:
            metrics(browser(system(metadata)))
