# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import sys

from structuredlog import StructuredLogger
import handlers
import formatters

log_formatters = {
    'raw': (formatters.JSONFormatter, "Raw structured log messages"),
    'unittest': (formatters.UnittestFormatter, "Unittest style output"),
    'xunit': (formatters.XUnitFormatter, "xUnit compatible XML"),
    'html': (formatters.HTMLFormatter, "HTML report"),
    'mach': (formatters.MachFormatter, "Uncolored mach-like output"),
    'mach_terminal': (formatters.MachTerminalFormatter, "Colored mach-like output for use in a tty"),
}


def log_file(name):
    if name == "-":
        return sys.stdout
    else:
        return open(name, "w")


def add_logging_group(parser):
    """
    Add logging options to an argparse ArgumentParser.

    Each formatter has a corresponding option of the form --log-{name}
    where {name} is the name of the formatter. The option takes a value
    which is either a filename or "-" to indicate stdout.

    :param parser: The ArgumentParser object that should have logging
                   options added.
    """
    group = parser.add_argument_group("Output Logging",
                                      description="Options for logging output.\n"
                                      "Each option represents a possible logging format "
                                      "and takes a filename to write that format to, "
                                      "or '-' to write to stdout.")
    for name, (cls, help_str) in log_formatters.iteritems():
        group.add_argument("--log-" + name, action="append", type=log_file,
                           help=help_str)


def setup_logging(suite, args, defaults):
    """
    Configure a structuredlogger based on command line arguments.

    :param suite: The name of the testsuite being run
    :param args: A dictionary of {argument_name:value} produced from
                 parsing the command line arguments for the application
    :param defaults: A dictionary of {formatter name: output stream} to apply
                     when there is no logging supplied on the command line.

    :rtype: StructuredLogger
    """
    logger = StructuredLogger(suite)
    prefix = "log_"
    found = False
    found_stdout_logger = False
    for name, values in args.iteritems():
        if name.startswith(prefix) and values is not None:
            for value in values:
                found = True
                if value == sys.stdout:
                    found_stdout_logger = True
                formatter_cls = log_formatters[name[len(prefix):]][0]
                logger.add_handler(handlers.StreamHandler(stream=value,
                                                          formatter=formatter_cls()))

    #If there is no user-specified logging, go with the default options
    if not found:
        for name, value in defaults.iteritems():
            formatter_cls = log_formatters[name][0]
            logger.add_handler(handlers.StreamHandler(stream=value,
                                                      formatter=formatter_cls()))

    elif not found_stdout_logger and sys.stdout in defaults.values():
        for name, value in defaults.iteritems():
            if value == sys.stdout:
                formatter_cls = log_formatters[name][0]
                logger.add_handler(handlers.StreamHandler(stream=value,
                                                          formatter=formatter_cls()))

    return logger
