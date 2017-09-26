# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import os
import sys

from mozprofile import MozProfileCLI

from .application import get_app_context
from .runners import runners
from .utils import findInPath

# Map of debugging programs to information about them
# from http://mxr.mozilla.org/mozilla-central/source/build/automationutils.py#59
DEBUGGERS = {'gdb': {'interactive': True,
                     'args': ['-q', '--args'], },
             'valgrind': {'interactive': False,
                          'args': ['--leak-check=full']}
             }


def debugger_arguments(debugger, arguments=None, interactive=None):
    """Finds debugger arguments from debugger given and defaults

    :param debugger: name or path to debugger
    :param arguments: arguments for the debugger, or None to use defaults
    :param interactive: whether the debugger should run in interactive mode

    """
    # find debugger executable if not a file
    executable = debugger
    if not os.path.exists(executable):
        executable = findInPath(debugger)
    if executable is None:
        raise Exception("Path to '%s' not found" % debugger)

    # if debugger not in dictionary of knowns return defaults
    dirname, debugger = os.path.split(debugger)
    if debugger not in DEBUGGERS:
        return ([executable] + (arguments or []), bool(interactive))

    # otherwise use the dictionary values for arguments unless specified
    if arguments is None:
        arguments = DEBUGGERS[debugger].get('args', [])
    if interactive is None:
        interactive = DEBUGGERS[debugger].get('interactive', False)
    return ([executable] + arguments, interactive)


class CLI(MozProfileCLI):
    """Command line interface"""

    module = "mozrunner"

    def __init__(self, args=sys.argv[1:]):
        MozProfileCLI.__init__(self, args=args)

        # choose appropriate runner and profile classes
        app = self.options.app
        try:
            self.runner_class = runners[app]
            self.profile_class = get_app_context(app).profile_class
        except KeyError:
            self.parser.error('Application "%s" unknown (should be one of "%s")' %
                              (app, ', '.join(runners.keys())))

    def add_options(self, parser):
        """add options to the parser"""
        parser.description = ("Reliable start/stop/configuration of Mozilla"
                              " Applications (Firefox, Thunderbird, etc.)")

        # add profile options
        MozProfileCLI.add_options(self, parser)

        # add runner options
        parser.add_option('-b', "--binary",
                          dest="binary", help="Binary path.",
                          metavar=None, default=None)
        parser.add_option('--app', dest='app', default='firefox',
                          help="Application to use [DEFAULT: %default]")
        parser.add_option('--app-arg', dest='appArgs',
                          default=[], action='append',
                          help="provides an argument to the test application")
        parser.add_option('--debugger', dest='debugger',
                          help="run under a debugger, e.g. gdb or valgrind")
        parser.add_option('--debugger-args', dest='debugger_args',
                          action='store',
                          help="arguments to the debugger")
        parser.add_option('--interactive', dest='interactive',
                          action='store_true',
                          help="run the program interactively")

    # methods for running

    def command_args(self):
        """additional arguments for the mozilla application"""
        return map(os.path.expanduser, self.options.appArgs)

    def runner_args(self):
        """arguments to instantiate the runner class"""
        return dict(cmdargs=self.command_args(),
                    binary=self.options.binary)

    def create_runner(self):
        profile = self.profile_class(**self.profile_args())
        return self.runner_class(profile=profile, **self.runner_args())

    def run(self):
        runner = self.create_runner()
        self.start(runner)
        runner.cleanup()

    def debugger_arguments(self):
        """Get the debugger arguments

        returns a 2-tuple of debugger arguments:
            (debugger_arguments, interactive)

        """
        debug_args = self.options.debugger_args
        if debug_args is not None:
            debug_args = debug_args.split()
        interactive = self.options.interactive
        if self.options.debugger:
            debug_args, interactive = debugger_arguments(self.options.debugger, debug_args,
                                                         interactive)
        return debug_args, interactive

    def start(self, runner):
        """Starts the runner and waits for the application to exit

        It can also happen via a keyboard interrupt. It should be
        overwritten to provide custom running of the runner instance.

        """
        # attach a debugger if specified
        debug_args, interactive = self.debugger_arguments()
        runner.start(debug_args=debug_args, interactive=interactive)
        print('Starting: ' + ' '.join(runner.command))
        try:
            runner.wait()
        except KeyboardInterrupt:
            runner.stop()


def cli(args=sys.argv[1:]):
    CLI(args).run()


if __name__ == '__main__':
    cli()
