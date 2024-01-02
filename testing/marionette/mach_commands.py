# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import functools
import logging
import os
import sys

from six import iteritems

from mach.decorators import (
    Command,
)

from mozbuild.base import (
    MachCommandConditions as conditions,
    BinaryNotFoundException,
)

SUPPORTED_APPS = ["firefox", "android", "thunderbird"]


def create_parser_tests():
    from marionette_harness.runtests import MarionetteArguments
    from mozlog.structured import commandline

    parser = MarionetteArguments()
    commandline.add_logging_group(parser)
    return parser


def run_marionette(tests, binary=None, topsrcdir=None, **kwargs):
    from mozlog.structured import commandline

    from marionette_harness.runtests import MarionetteTestRunner, MarionetteHarness

    parser = create_parser_tests()

    args = argparse.Namespace(tests=tests)

    args.binary = binary
    args.logger = kwargs.pop("log", None)

    for k, v in iteritems(kwargs):
        setattr(args, k, v)

    parser.verify_usage(args)

    # Causes Firefox to crash when using non-local connections.
    os.environ["MOZ_DISABLE_NONLOCAL_CONNECTIONS"] = "1"

    if not args.logger:
        args.logger = commandline.setup_logging(
            "Marionette Unit Tests", args, {"mach": sys.stdout}
        )
    failed = MarionetteHarness(MarionetteTestRunner, args=vars(args)).run()
    if failed > 0:
        return 1
    else:
        return 0


@Command(
    "marionette-test",
    category="testing",
    description="Remote control protocol to Gecko, used for browser automation.",
    conditions=[functools.partial(conditions.is_buildapp_in, apps=SUPPORTED_APPS)],
    parser=create_parser_tests,
)
def marionette_test(command_context, tests, **kwargs):
    if "test_objects" in kwargs:
        tests = []
        for obj in kwargs["test_objects"]:
            tests.append(obj["file_relpath"])
        del kwargs["test_objects"]

    if not tests:
        if conditions.is_thunderbird(command_context):
            tests = [
                os.path.join(
                    command_context.topsrcdir,
                    "comm/testing/marionette/unit-tests.ini",
                )
            ]
        else:
            tests = [
                os.path.join(
                    command_context.topsrcdir,
                    "testing/marionette/harness/marionette_harness/tests/unit-tests.toml",
                )
            ]

    if not kwargs.get("binary") and (
        conditions.is_firefox(command_context)
        or conditions.is_thunderbird(command_context)
    ):
        try:
            kwargs["binary"] = command_context.get_binary_path("app")
        except BinaryNotFoundException as e:
            command_context.log(
                logging.ERROR,
                "marionette-test",
                {"error": str(e)},
                "ERROR: {error}",
            )
            command_context.log(
                logging.INFO, "marionette-test", {"help": e.help()}, "{help}"
            )
            return 1

    return run_marionette(tests, topsrcdir=command_context.topsrcdir, **kwargs)
