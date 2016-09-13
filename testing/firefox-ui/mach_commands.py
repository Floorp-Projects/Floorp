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
    Command,
    CommandProvider,
)


def setup_argument_parser_functional():
    from firefox_ui_harness.arguments.base import FirefoxUIArguments
    from mozlog.structured import commandline
    parser = FirefoxUIArguments()
    commandline.add_logging_group(parser)
    return parser


def setup_argument_parser_update():
    from firefox_ui_harness.arguments.update import UpdateArguments
    from mozlog.structured import commandline
    parser = UpdateArguments()
    commandline.add_logging_group(parser)
    return parser


def run_firefox_ui_test(testtype=None, topsrcdir=None, **kwargs):
    from mozlog.structured import commandline
    from argparse import Namespace
    import firefox_ui_harness

    if testtype == 'functional':
        parser = setup_argument_parser_functional()
    else:
        parser = setup_argument_parser_update()

    test_types = {
        'functional': {
            'default_tests': [
                os.path.join('puppeteer', 'manifest.ini'),
                os.path.join('functional', 'manifest.ini'),
            ],
            'cli_module': firefox_ui_harness.cli_functional,
        },
        'update': {
            'default_tests': [
                os.path.join('update', 'manifest.ini'),
            ],
            'cli_module': firefox_ui_harness.cli_update,
        }
    }

    fxui_dir = os.path.join(topsrcdir, 'testing', 'firefox-ui')

    # Set the resources path which is used to serve test data via wptserve
    if not kwargs['server_root']:
        kwargs['server_root'] = os.path.join(fxui_dir, 'resources')

    # If called via "mach test" a dictionary of tests is passed in
    if 'test_objects' in kwargs:
        tests = []
        for obj in kwargs['test_objects']:
            tests.append(obj['file_relpath'])
        kwargs['tests'] = tests
    elif not kwargs.get('tests'):
        # If no tests have been selected, set default ones
        kwargs['tests'] = [os.path.join(fxui_dir, 'tests', test)
                           for test in test_types[testtype]['default_tests']]

    kwargs['logger'] = commandline.setup_logging('Firefox UI - {} Tests'.format(testtype),
                                                 {"mach": sys.stdout})

    args = Namespace()

    for k, v in kwargs.iteritems():
        setattr(args, k, v)

    parser.verify_usage(args)

    failed = test_types[testtype]['cli_module'].cli(args=vars(args))

    if failed > 0:
        return 1
    else:
        return 0


@CommandProvider
class MachCommands(MachCommandBase):
    """Mach command provider for Firefox ui tests."""

    @Command('firefox-ui-functional', category='testing',
             conditions=[conditions.is_firefox],
             description='Run the functional test suite of Firefox UI tests.',
             parser=setup_argument_parser_functional,
             )
    def run_firefox_ui_functional(self, **kwargs):
        kwargs['binary'] = kwargs['binary'] or self.get_binary_path('app')
        return run_firefox_ui_test(testtype='functional',
                                   topsrcdir=self.topsrcdir, **kwargs)

    @Command('firefox-ui-update', category='testing',
             conditions=[conditions.is_firefox],
             description='Run the update test suite of Firefox UI tests.',
             parser=setup_argument_parser_update,
             )
    def run_firefox_ui_update(self, **kwargs):
        kwargs['binary'] = kwargs['binary'] or self.get_binary_path('app')
        return run_firefox_ui_test(testtype='update',
                                   topsrcdir=self.topsrcdir, **kwargs)
