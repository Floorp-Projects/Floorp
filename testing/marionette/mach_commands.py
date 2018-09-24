# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import os
import sys

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
    SubCommand,
)

from mozbuild.base import (
    MachCommandBase,
    MachCommandConditions as conditions,
)


def create_parser_tests():
    from marionette_harness.runtests import MarionetteArguments
    from mozlog.structured import commandline
    parser = MarionetteArguments()
    commandline.add_logging_group(parser)
    return parser


def run_marionette(tests, binary=None, topsrcdir=None, **kwargs):
    from mozlog.structured import commandline

    from marionette_harness.runtests import (
        MarionetteTestRunner,
        MarionetteHarness
    )

    parser = create_parser_tests()

    if not tests:
        tests = [os.path.join(topsrcdir,
                 "testing/marionette/harness/marionette_harness/tests/unit-tests.ini")]

    args = argparse.Namespace(tests=tests)

    args.binary = binary
    args.logger = kwargs.pop('log', None)

    for k, v in kwargs.iteritems():
        setattr(args, k, v)

    parser.verify_usage(args)

    if not args.logger:
        args.logger = commandline.setup_logging("Marionette Unit Tests",
                                                args,
                                                {"mach": sys.stdout})
    failed = MarionetteHarness(MarionetteTestRunner, args=vars(args)).run()
    if failed > 0:
        return 1
    else:
        return 0


@CommandProvider
class MarionetteTest(MachCommandBase):
    @Command("marionette-test",
             category="testing",
             description="Remote control protocol to Gecko, used for functional UI tests and browser automation.",
             conditions=[conditions.is_firefox_or_android],
             parser=create_parser_tests,
             )
    def marionette_test(self, tests, **kwargs):
        if "test_objects" in kwargs:
            tests = []
            for obj in kwargs["test_objects"]:
                tests.append(obj["file_relpath"])
            del kwargs["test_objects"]

        if not kwargs.get("binary") and conditions.is_firefox(self):
            kwargs["binary"] = self.get_binary_path("app")
        return run_marionette(tests, topsrcdir=self.topsrcdir, **kwargs)


@CommandProvider
class Marionette(MachCommandBase):

    @property
    def srcdir(self):
        return os.path.join(self.topsrcdir, "testing/marionette")

    @Command("marionette",
             category="misc",
             description="Remote control protocol to Gecko, used for functional UI tests and browser automation.",
             conditions=[conditions.is_firefox_or_android],
             )
    def marionette(self):
        self.parser.print_usage()
        return 1

    @SubCommand("marionette", "test",
                description="Run browser automation tests based on Marionette harness.",
                parser=create_parser_tests,
                )
    def marionette_test(self, tests, **kwargs):
        if "test_objects" in kwargs:
            tests = []
            for obj in kwargs["test_objects"]:
                tests.append(obj["file_relpath"])
            del kwargs["test_objects"]

        if not kwargs.get("binary") and conditions.is_firefox(self):
            kwargs["binary"] = self.get_binary_path("app")
        return run_marionette(tests, topsrcdir=self.topsrcdir, **kwargs)
