# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys

from marionette_test import MarionetteTestCase, MarionetteJSTestCase
from mozlog import structured
from runner import BaseMarionetteTestRunner, BaseMarionetteOptions

class MarionetteTbplFormatter(structured.formatters.TbplFormatter):
    """Formatter that logs failures in a format that agrees with legacy
    log data used by tbpl."""

    def test_end(self, data):
        # TBPL ui expects relevant info about an exception to appear in the first
        # line of the log message, however tracebacks provided by marionette
        # put this information on the last line.
        if "message" in data:
            message_lines = [line for line in data["message"].splitlines() if line != ""]
            if "Traceback" in message_lines[0]:
                exc_msg_index = None
                for index, line in enumerate(message_lines):
                    if "Error: " in line or "Exception: " in line:
                        exc_msg_index = index
                        break
                if exc_msg_index:
                    message_lines = (message_lines[exc_msg_index:] +
                                     message_lines[:exc_msg_index])
            data["message"] = "\n".join(message_lines)
        return super(MarionetteTbplFormatter, self).test_end(data)

class MarionetteTestRunner(BaseMarionetteTestRunner):
    def __init__(self, **kwargs):
        BaseMarionetteTestRunner.__init__(self, **kwargs)
        self.test_handlers = [MarionetteTestCase, MarionetteJSTestCase]

def startTestRunner(runner_class, options, tests):

    runner = runner_class(**vars(options))
    runner.run_tests(tests)
    return runner


def cli(runner_class=MarionetteTestRunner, parser_class=BaseMarionetteOptions):
    parser = parser_class(usage='%prog [options] test_file_or_dir <test_file_or_dir> ...')
    structured.commandline.add_logging_group(parser)
    options, tests = parser.parse_args()
    parser.verify_usage(options, tests)

    logger = structured.commandline.setup_logging(
        options.logger_name, options, {})

    # Only add the tbpl logger if a handler isn't already logging to stdout
    has_stdout_logger = any([h.stream == sys.stdout for h in logger.handlers])
    if not has_stdout_logger:
        formatter = MarionetteTbplFormatter()
        handler = structured.handlers.StreamHandler(sys.stdout, formatter)
        logger.add_handler(structured.handlers.LogLevelFilter(
            handler, 'info'))

    options.logger = logger

    runner = startTestRunner(runner_class, options, tests)
    if runner.failed > 0:
        sys.exit(10)

if __name__ == "__main__":
    cli()
