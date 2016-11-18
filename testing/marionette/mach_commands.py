# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import os
import sys
import argparse

from mozbuild.base import (
    MachCommandBase,
    MachCommandConditions as conditions,
)

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

def is_firefox_or_android(cls):
    """Must have Firefox build or Android build."""
    return conditions.is_firefox(cls) or conditions.is_android(cls)

def setup_marionette_argument_parser():
    from marionette.runtests import MarionetteArguments
    from mozlog.structured import commandline
    parser = MarionetteArguments()
    commandline.add_logging_group(parser)
    return parser

def run_marionette(tests, binary=None, topsrcdir=None, **kwargs):
    from mozlog.structured import commandline

    from marionette.runtests import (
        MarionetteTestRunner,
        MarionetteHarness
    )

    parser = setup_marionette_argument_parser()

    if not tests:
        tests = [os.path.join(topsrcdir,
                 'testing/marionette/harness/marionette/tests/unit-tests.ini')]

    args = argparse.Namespace(tests=tests)

    args.binary = binary

    for k, v in kwargs.iteritems():
        setattr(args, k, v)

    parser.verify_usage(args)

    args.logger = commandline.setup_logging("Marionette Unit Tests",
                                            args,
                                            {"mach": sys.stdout})
    failed = MarionetteHarness(MarionetteTestRunner, args=vars(args)).run()
    if failed > 0:
        return 1
    else:
        return 0

@CommandProvider
class MachCommands(MachCommandBase):
    @Command('marionette-test', category='testing',
        description='Run a Marionette test (Check UI or the internal JavaScript using marionette).',
        conditions=[is_firefox_or_android],
        parser=setup_marionette_argument_parser,
    )
    def run_marionette_test(self, tests, **kwargs):
        if 'test_objects' in kwargs:
            tests = []
            for obj in kwargs['test_objects']:
                tests.append(obj['file_relpath'])
            del kwargs['test_objects']

        if not kwargs.get('binary') and conditions.is_firefox(self):
            kwargs['binary'] = self.get_binary_path('app')
        return run_marionette(tests, topsrcdir=self.topsrcdir, **kwargs)
