# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import logging
import os
import sys

from mach.decorators import Command
from mozbuild.base import BinaryNotFoundException
from mozbuild.base import MachCommandConditions as conditions


def create_parser_tests():
    from marionette_harness.runtests import MarionetteArguments
    from mozlog.structured import commandline

    parser = MarionetteArguments()
    commandline.add_logging_group(parser)
    return parser


def run_telemetry(tests, binary=None, topsrcdir=None, **kwargs):
    from marionette_harness.runtests import MarionetteHarness
    from mozlog.structured import commandline
    from telemetry_harness.runtests import TelemetryTestRunner

    parser = create_parser_tests()

    if not tests:
        tests = [
            os.path.join(
                topsrcdir,
                "toolkit/components/telemetry/tests/marionette/tests/manifest.toml",
            )
        ]

    args = argparse.Namespace(tests=tests)

    args.binary = binary
    args.logger = kwargs.pop("log", None)

    for k, v in kwargs.items():
        setattr(args, k, v)

    parser.verify_usage(args)

    os.environ["MOZ_IGNORE_NSS_SHUTDOWN_LEAKS"] = "1"

    # Causes Firefox to crash when using non-local connections.
    os.environ["MOZ_DISABLE_NONLOCAL_CONNECTIONS"] = "1"

    if not args.logger:
        args.logger = commandline.setup_logging(
            "Telemetry Client Tests", args, {"mach": sys.stdout}
        )
    failed = MarionetteHarness(TelemetryTestRunner, args=vars(args)).run()
    if failed > 0:
        return 1
    return 0


@Command(
    "telemetry-tests-client",
    category="testing",
    description="Run tests specifically for the Telemetry client",
    conditions=[conditions.is_firefox_or_android],
    parser=create_parser_tests,
)
def telemetry_test(command_context, tests, **kwargs):
    if "test_objects" in kwargs:
        tests = []
        for obj in kwargs["test_objects"]:
            tests.append(obj["file_relpath"])
        del kwargs["test_objects"]
    if not kwargs.get("binary") and conditions.is_firefox(command_context):
        try:
            kwargs["binary"] = command_context.get_binary_path("app")
        except BinaryNotFoundException as e:
            command_context.log(
                logging.ERROR,
                "telemetry-tests-client",
                {"error": str(e)},
                "ERROR: {error}",
            )
            command_context.log(
                logging.INFO, "telemetry-tests-client", {"help": e.help()}, "{help}"
            )
            return 1
    if not kwargs.get("server_root"):
        kwargs[
            "server_root"
        ] = "toolkit/components/telemetry/tests/marionette/harness/www"
    return run_telemetry(tests, topsrcdir=command_context.topsrcdir, **kwargs)
