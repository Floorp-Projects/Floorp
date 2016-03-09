# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import os
import sys

from mozbuild.base import (
    MachCommandBase,
    MachCommandConditions as conditions,
)

from mach.decorators import (
    CommandProvider,
    Command,
)


def setup_argument_parser():
    from firefox_ui_harness.arguments.base import FirefoxUIArguments
    return FirefoxUIArguments()


def run_firefox_ui_test(tests, testtype=None, topsrcdir=None, **kwargs):
    from mozlog.structured import commandline
    from firefox_ui_harness import cli_functional
    from firefox_ui_harness.arguments import FirefoxUIArguments

    parser = FirefoxUIArguments()
    commandline.add_logging_group(parser)

    if not tests:
        tests = [os.path.join(topsrcdir,
                 'testing/firefox-ui/tests/firefox_ui_tests/manifest.ini')]

    args = parser.parse_args(args=tests)

    for k, v in kwargs.iteritems():
        setattr(args, k, v)

    parser.verify_usage(args)

    args.logger = commandline.setup_logging("Firefox UI - Functional Tests",
                                            args,
                                            {"mach": sys.stdout})
    failed = cli_functional.mn_cli(cli_functional.FirefoxUITestRunner, args=args)
    if failed > 0:
        return 1
    else:
        return 0


@CommandProvider
class MachCommands(MachCommandBase):
    @Command('firefox-ui-test', category='testing',
             description='Run Firefox UI functional tests.',
             conditions=[conditions.is_firefox],
             parser=setup_argument_parser,
             )
    def run_firefox_ui_test(self, tests, **kwargs):
        kwargs['binary'] = kwargs['binary'] or self.get_binary_path('app')
        return run_firefox_ui_test(tests, topsrcdir=self.topsrcdir, **kwargs)
