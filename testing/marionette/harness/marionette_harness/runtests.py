# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import sys

import mozlog

from marionette_driver import __version__ as driver_version

from marionette_harness import (
    __version__,
    BaseMarionetteTestRunner,
    BaseMarionetteArguments,
    MarionetteTestCase,
)


class MarionetteTestRunner(BaseMarionetteTestRunner):
    def __init__(self, **kwargs):
        BaseMarionetteTestRunner.__init__(self, **kwargs)
        self.test_handlers = [MarionetteTestCase]


class MarionetteArguments(BaseMarionetteArguments):
    pass


class MarionetteHarness(object):
    def __init__(self,
                 runner_class=MarionetteTestRunner,
                 parser_class=MarionetteArguments,
                 testcase_class=MarionetteTestCase,
                 args=None):
        self._runner_class = runner_class
        self._parser_class = parser_class
        self._testcase_class = testcase_class
        self.args = args or self.parse_args()

    def parse_args(self, logger_defaults=None):
        parser = self._parser_class(
            usage='%(prog)s [options] test_file_or_dir <test_file_or_dir> ...')
        parser.add_argument('--version', action='version',
                            help="Show version information.",
                            version="%(prog)s {version}"
                            " (using marionette-driver: {driver_version}, ".format(
                                version=__version__,
                                driver_version=driver_version
                            ))
        mozlog.commandline.add_logging_group(parser)
        args = parser.parse_args()
        parser.verify_usage(args)

        logger = mozlog.commandline.setup_logging(
            args.logger_name, args, logger_defaults or {"tbpl": sys.stdout})

        args.logger = logger
        return vars(args)

    def process_args(self):
        if self.args.get('pydebugger'):
            self._testcase_class.pydebugger = __import__(self.args['pydebugger'])

    def run(self):
        self.process_args()
        tests = self.args.pop('tests')
        runner = self._runner_class(**self.args)
        try:
            runner.run_tests(tests)
        finally:
            runner.cleanup()
        return runner.failed + runner.crashed


def cli(runner_class=MarionetteTestRunner, parser_class=MarionetteArguments,
        harness_class=MarionetteHarness, testcase_class=MarionetteTestCase, args=None):
    """
    Call the harness to parse args and run tests.

    The following exit codes are expected:
    - Test failures: 10
    - Harness/other failures: 1
    - Success: 0
    """
    logger = mozlog.commandline.setup_logging('Marionette test runner', {})
    try:
        harness_instance = harness_class(runner_class, parser_class, testcase_class,
                                         args=args)
        failed = harness_instance.run()
        if failed > 0:
            sys.exit(10)
    except Exception as e:
        logger.error(e.message, exc_info=True)
        sys.exit(1)
    sys.exit(0)


if __name__ == "__main__":
    cli()
