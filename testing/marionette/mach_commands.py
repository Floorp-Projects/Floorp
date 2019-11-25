# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import os
import sys

from six import iteritems

from mach.decorators import (
    CommandProvider,
    Command,
)

from mozbuild.base import (
    MachCommandBase,
    MachCommandConditions as conditions,
)

SUPPORTED_APPS = ['firefox', 'android', 'thunderbird']

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

    args = argparse.Namespace(tests=tests)

    args.binary = binary
    args.logger = kwargs.pop('log', None)

    for k, v in iteritems(kwargs):
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


def is_buildapp_in(*apps):
    def is_buildapp_supported(cls):
        for a in apps:
            c = getattr(conditions, 'is_{}'.format(a), None)
            if c and c(cls):
                return True
        return False

    is_buildapp_supported.__doc__ = 'Must have a {} build.'.format(
        ' or '.join(apps))
    return is_buildapp_supported


@CommandProvider
class MarionetteTest(MachCommandBase):
    @Command("marionette-test",
             category="testing",
             description="Remote control protocol to Gecko, used for browser automation.",
             conditions=[is_buildapp_in(*SUPPORTED_APPS)],
             parser=create_parser_tests,
             )
    def marionette_test(self, tests, **kwargs):
        if "test_objects" in kwargs:
            tests = []
            for obj in kwargs["test_objects"]:
                tests.append(obj["file_relpath"])
            del kwargs["test_objects"]

        if not tests:
            if conditions.is_thunderbird(self):
                tests = [os.path.join(self.topsrcdir,
                         "comm/testing/marionette/unit-tests.ini")]
            else:
                tests = [os.path.join(self.topsrcdir,
                         "testing/marionette/harness/marionette_harness/tests/unit-tests.ini")]

        # Force disable e10s because it is not supported in Fennec
        if kwargs.get("app") == "fennec":
            kwargs["e10s"] = False

        if not kwargs.get("binary") and \
                (conditions.is_firefox(self) or conditions.is_thunderbird(self)):
            kwargs["binary"] = self.get_binary_path("app")

        return run_marionette(tests, topsrcdir=self.topsrcdir, **kwargs)
