# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys

from marionette import __version__
from marionette_driver import __version__ as driver_version
from marionette_transport import __version__ as transport_version
from marionette.marionette_test import MarionetteTestCase, MarionetteJSTestCase
from marionette.runner import (
    BaseMarionetteTestRunner,
    BaseMarionetteArguments,
    BrowserMobProxyArguments,
)
import mozlog


class MarionetteTestRunner(BaseMarionetteTestRunner):
    def __init__(self, **kwargs):
        BaseMarionetteTestRunner.__init__(self, **kwargs)
        self.test_handlers = [MarionetteTestCase, MarionetteJSTestCase]


class MarionetteArguments(BaseMarionetteArguments):
    def __init__(self, **kwargs):
        BaseMarionetteArguments.__init__(self, **kwargs)
        self.register_argument_container(BrowserMobProxyArguments())


def startTestRunner(runner_class, args):
    if args.pydebugger:
        MarionetteTestCase.pydebugger = __import__(args.pydebugger)

    args = vars(args)
    tests = args.pop('tests')
    runner = runner_class(**args)
    runner.run_tests(tests)
    return runner

def cli(runner_class=MarionetteTestRunner, parser_class=MarionetteArguments):
    parser = parser_class(
        usage='%prog [options] test_file_or_dir <test_file_or_dir> ...',
        version="%prog {version} (using marionette-driver: {driver_version}"
                ", marionette-transport: {transport_version})".format(
                    version=__version__,
                    driver_version=driver_version,
                    transport_version=transport_version)
    )
    mozlog.commandline.add_logging_group(parser)
    args = parser.parse_args()
    parser.verify_usage(args)

    logger = mozlog.commandline.setup_logging(
        args.logger_name, args, {"tbpl": sys.stdout})

    args.logger = logger

    runner = startTestRunner(runner_class, args)
    if runner.failed > 0:
        sys.exit(10)

if __name__ == "__main__":
    cli()
