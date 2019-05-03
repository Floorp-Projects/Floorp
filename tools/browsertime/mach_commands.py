# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

r'''Make it easy to install and run [browsertime](https://github.com/sitespeedio/browsertime).

Browsertime is a harness for running performance tests, similar to
Mozilla's Raptor testing framework.  Browsertime is written in Node.js
and uses Selenium WebDriver to drive multiple browsers including
Chrome, Chrome for Android, Firefox, and (pending the resolution of
[Bug 1525126](https://bugzilla.mozilla.org/show_bug.cgi?id=1525126)
and similar tickets) Firefox for Android and GeckoView-based vehicles.

Right now a custom version of browsertime and the underlying
geckodriver binary are needed to support GeckoView-based vehicles;
this module accommodates those in-progress custom versions.

To get started, run
```
./mach browsertime --setup [--clobber]
```
This will populate `tools/browsertime/node_modules`.

To invoke browsertime, run
```
./mach browsertime [ARGS]
```
All arguments are passed through to browsertime.
'''

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import logging
import os
import sys

import mozpack.path as mozpath
from mach.decorators import CommandArgument, CommandProvider, Command
from mozbuild.base import MachCommandBase
from mozbuild.nodeutil import find_node_executable

sys.path.append(mozpath.join(os.path.dirname(__file__), '..', '..', 'tools', 'lint', 'eslint'))
import setup_helper


@CommandProvider
class MachBrowsertime(MachCommandBase):
    def setup(self, should_clobber=False):
        if not setup_helper.check_node_executables_valid():
            return 1

        return setup_helper.package_setup(
            os.path.dirname(__file__),
            'browsertime',
            should_clobber=should_clobber)

    def node(self, args):
        node, _ = find_node_executable()

        # Ensure that bare `node` and `npm` in scripts, including post-install
        # scripts, finds the binary we're invoking with.  Without this, it's
        # easy for compiled extensions to get mismatched versions of the Node.js
        # extension API.
        path = os.environ.get('PATH', '').split(os.pathsep)
        node_path = os.path.dirname(node)
        if node_path not in path:
            path = [node_path] + path

        return self.run_process(
            [node] + args,
            append_env={'PATH': os.pathsep.join(path)},
            pass_thru=True,  # Allow user to run Node interactively.
            ensure_exit_code=False,  # Don't throw on non-zero exit code.
            cwd=mozpath.join(self.topsrcdir))

    def bin_path(self):
        # On Windows, invoking `node_modules/.bin/browsertime{.cmd}`
        # doesn't work when invoked as an argument to our specific
        # binary.  Since we want our version of node, invoke the
        # actual script directly.
        return mozpath.join(
            os.path.dirname(__file__),
            'node_modules',
            'browsertime',
            'bin',
            'browsertime.js')

    @Command('browsertime', category='testing',
             description='Run [browsertime](https://github.com/sitespeedio/browsertime) '
                         'performance tests.')
    @CommandArgument('--verbose', action='store_true',
                     help='Verbose output for what commands the build is running.')
    @CommandArgument('--setup', default=False, action='store_true')
    @CommandArgument('--clobber', default=False, action='store_true')
    @CommandArgument('args', nargs=argparse.REMAINDER)
    def browsertime(self, args, verbose=False, setup=False, clobber=False):
        if setup:
            return self.setup(should_clobber=clobber)

        if not verbose:
            # Avoid logging the command
            self.log_manager.terminal_handler.setLevel(logging.CRITICAL)

        return self.node([self.bin_path()] + args)
