# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys

from marionette import __version__
from marionette.marionette_test import MarionetteTestCase, MarionetteJSTestCase
from mozlog import structured
from marionette.runner import (
    BaseMarionetteTestRunner,
    BaseMarionetteOptions,
    BrowserMobProxyOptionsMixin
    )


class MarionetteTestRunner(BaseMarionetteTestRunner):
    def __init__(self, **kwargs):
        BaseMarionetteTestRunner.__init__(self, **kwargs)
        self.test_handlers = [MarionetteTestCase, MarionetteJSTestCase]


class MarionetteOptions(BaseMarionetteOptions,
                        BrowserMobProxyOptionsMixin):
    def __init__(self, **kwargs):
        BaseMarionetteOptions.__init__(self, **kwargs)
        BrowserMobProxyOptionsMixin.__init__(self, **kwargs)


def startTestRunner(runner_class, options, tests):
    if options.pydebugger:
        MarionetteTestCase.pydebugger = __import__(options.pydebugger)

    runner = runner_class(**vars(options))
    runner.run_tests(tests)
    return runner

def cli(runner_class=MarionetteTestRunner, parser_class=MarionetteOptions):
    parser = parser_class(usage='%prog [options] test_file_or_dir <test_file_or_dir> ...',
                          version='%prog ' + __version__)
    structured.commandline.add_logging_group(parser)
    options, tests = parser.parse_args()
    parser.verify_usage(options, tests)

    logger = structured.commandline.setup_logging(
        options.logger_name, options, {"tbpl": sys.stdout})

    options.logger = logger

    runner = startTestRunner(runner_class, options, tests)
    if runner.failed > 0:
        sys.exit(10)

if __name__ == "__main__":
    cli()
