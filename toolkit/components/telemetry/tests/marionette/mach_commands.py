# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import, print_function, unicode_literals

import argparse
import logging
import os
import sys

from mach.decorators import CommandProvider, Command

from mozbuild.base import (
    MachCommandBase,
    MachCommandConditions as conditions,
    BinaryNotFoundException,
)


def create_parser_tests():
    from marionette_harness.runtests import MarionetteArguments
    from mozlog.structured import commandline

    parser = MarionetteArguments()
    commandline.add_logging_group(parser)
    return parser


def run_telemetry(tests, binary=None, topsrcdir=None, **kwargs):
    from mozlog.structured import commandline

    from telemetry_harness.runtests import TelemetryTestRunner

    from marionette_harness.runtests import MarionetteHarness

    parser = create_parser_tests()

    if not tests:
        tests = [
            os.path.join(
                topsrcdir,
                "toolkit/components/telemetry/tests/marionette/tests/manifest.ini",
            )
        ]

    args = argparse.Namespace(tests=tests)

    args.binary = binary
    args.logger = kwargs.pop("log", None)

    for k, v in kwargs.iteritems():
        setattr(args, k, v)

    parser.verify_usage(args)

    if not args.logger:
        args.logger = commandline.setup_logging(
            "Telemetry Client Tests", args, {"mach": sys.stdout}
        )
    failed = MarionetteHarness(TelemetryTestRunner, args=vars(args)).run()
    if failed > 0:
        return 1
    else:
        return 0


@CommandProvider
class TelemetryTest(MachCommandBase):
    @Command(
        "telemetry-tests-client",
        category="testing",
        description="Run tests specifically for the Telemetry client",
        conditions=[conditions.is_firefox_or_android],
        parser=create_parser_tests,
    )
    def telemetry_test(self, tests, **kwargs):
        if "test_objects" in kwargs:
            tests = []
            for obj in kwargs["test_objects"]:
                tests.append(obj["file_relpath"])
            del kwargs["test_objects"]
        if not kwargs.get("binary") and conditions.is_firefox(self):
            try:
                kwargs["binary"] = self.get_binary_path("app")
            except BinaryNotFoundException as e:
                self.log(logging.ERROR, 'telemetry-tests-client',
                         {'error': str(e)},
                         'ERROR: {error}')
                self.log(logging.INFO, 'telemetry-tests-client',
                         {'help': e.help()},
                         '{help}')
                return 1
        if not kwargs.get("server_root"):
            kwargs["server_root"] = "toolkit/components/telemetry/tests/marionette/harness/www"
        return run_telemetry(tests, topsrcdir=self.topsrcdir, **kwargs)
