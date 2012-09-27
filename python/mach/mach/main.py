# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This module provides functionality for the command-line build tool
# (mach). It is packaged as a module because everything is a library.

from __future__ import unicode_literals

import argparse
import logging
import os
import sys

from mozbuild.base import BuildConfig
from mozbuild.config import ConfigSettings
from mozbuild.logger import LoggingManager

# Import sub-command modules
# TODO Bug 794509 do this via auto-discovery. Update README once this is
# done.
from mach.build import Build
from mach.settings import Settings
from mach.testing import Testing
from mach.warnings import Warnings

# Classes inheriting from ArgumentProvider that provide commands.
HANDLERS = [
    Build,
    Settings,
    Testing,
    Warnings,
]

# Classes inheriting from ConfigProvider that provide settings.
# TODO this should come from auto-discovery somehow.
SETTINGS_PROVIDERS = [
    BuildConfig,
]

# Settings for argument parser that don't get proxied to sub-module. i.e. these
# are things consumed by the driver itself.
CONSUMED_ARGUMENTS = [
    'settings_file',
    'verbose',
    'logfile',
    'log_interval',
    'action',
    'cls',
    'method',
    'func',
]

class Mach(object):
    """Contains code for the command-line `mach` interface."""

    USAGE = """%(prog)s subcommand [arguments]

mach provides an interface to performing common developer tasks. You specify
an action/sub-command and it performs it.

Some common actions are:

    %(prog)s help      Show full help, including the list of all commands.
    %(prog)s test      Run tests.

To see more help for a specific action, run:

  %(prog)s <command> --help
"""

    def __init__(self, cwd):
        assert os.path.isdir(cwd)

        self.cwd = cwd
        self.log_manager = LoggingManager()
        self.logger = logging.getLogger(__name__)
        self.settings = ConfigSettings()

        self.log_manager.register_structured_logger(self.logger)

    def run(self, argv):
        """Runs mach with arguments provided from the command line."""
        parser = self.get_argument_parser()

        if not len(argv):
            # We don't register the usage until here because if it is globally
            # registered, argparse always prints it. This is not desired when
            # running with --help.
            parser.usage = Mach.USAGE
            parser.print_usage()
            return 0

        if argv[0] == 'help':
            parser.print_help()
            return 0

        args = parser.parse_args(argv)

        # Add JSON logging to a file if requested.
        if args.logfile:
            self.log_manager.add_json_handler(args.logfile)

        # Up the logging level if requested.
        log_level = logging.INFO
        if args.verbose:
            log_level = logging.DEBUG

        # Always enable terminal logging. The log manager figures out if we are
        # actually in a TTY or are a pipe and does the right thing.
        self.log_manager.add_terminal_logging(level=log_level,
            write_interval=args.log_interval)

        self.load_settings(args)
        conf = BuildConfig(self.settings)

        stripped = {k: getattr(args, k) for k in vars(args) if k not in
            CONSUMED_ARGUMENTS}

        # If the action is associated with a class, instantiate and run it.
        # All classes must be Base-derived and take the expected argument list.
        if hasattr(args, 'cls'):
            cls = getattr(args, 'cls')
            instance = cls(self.cwd, self.settings, self.log_manager)
            fn = getattr(instance, getattr(args, 'method'))

        # If the action is associated with a function, call it.
        elif hasattr(args, 'func'):
            fn = getattr(args, 'func')
        else:
            raise Exception('Dispatch configuration error in module.')

        fn(**stripped)

    def log(self, level, action, params, format_str):
        """Helper method to record a structured log event."""
        self.logger.log(level, format_str,
            extra={'action': action, 'params': params})

    def load_settings(self, args):
        """Determine which settings files apply and load them.

        Currently, we only support loading settings from a single file.
        Ideally, we support loading from multiple files. This is supported by
        the ConfigSettings API. However, that API currently doesn't track where
        individual values come from, so if we load from multiple sources then
        save, we effectively do a full copy. We don't want this. Until
        ConfigSettings does the right thing, we shouldn't expose multi-file
        loading.

        We look for a settings file in the following locations. The first one
        found wins:

          1) Command line argument
          2) Environment variable
          3) Default path
        """
        for provider in SETTINGS_PROVIDERS:
            provider.register_settings()
            self.settings.register_provider(provider)

        p = os.path.join(self.cwd, 'mach.ini')

        if args.settings_file:
            p = args.settings_file
        elif 'MACH_SETTINGS_FILE' in os.environ:
            p = os.environ['MACH_SETTINGS_FILE']

        self.settings.load_file(p)

        return os.path.exists(p)

    def get_argument_parser(self):
        """Returns an argument parser for the command-line interface."""

        parser = argparse.ArgumentParser()

        settings_group = parser.add_argument_group('Settings')
        settings_group.add_argument('--settings', dest='settings_file',
            metavar='FILENAME', help='Path to settings file.')

        logging_group = parser.add_argument_group('Logging')
        logging_group.add_argument('-v', '--verbose', dest='verbose',
            action='store_true', default=False,
            help='Print verbose output.')
        logging_group.add_argument('-l', '--log-file', dest='logfile',
            metavar='FILENAME', type=argparse.FileType('ab'),
            help='Filename to write log data to.')
        logging_group.add_argument('--log-interval', dest='log_interval',
            action='store_true', default=False,
            help='Prefix log line with interval from last message rather '
                'than relative time. Note that this is NOT execution time '
                'if there are parallel operations.')

        subparser = parser.add_subparsers(dest='action')

        # Register argument action providers with us.
        for cls in HANDLERS:
            cls.populate_argparse(subparser)

        return parser
