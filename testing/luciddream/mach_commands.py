# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Integrates luciddream test runner with mach.

import os
import re
import sys

import mozpack.path as mozpath

from mozbuild.base import (
    MachCommandBase,
    MachCommandConditions as conditions,
    MozbuildObject,
)

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

class LucidDreamRunner(MozbuildObject):
    """Run luciddream tests."""
    def run_tests(self, **kwargs):
        self._run_make(target='jetpack-tests')

@CommandProvider
class MachCommands(MachCommandBase):
    @Command('luciddream', category='testing',
        description='Runs the luciddream test suite.')
    @CommandArgument("--consoles", dest="consoles", action="store_true",
                     help="Open jsconsole in both runtimes.")
    @CommandArgument('--b2g-desktop', type=str, default=None,
                     help='Path to b2g desktop binary.')
    @CommandArgument('--firefox', type=str, default=None,
                     help='Path to firefox binary.')
    @CommandArgument('--gaia-profile', type=str, default=None,
                     help='Path to gaia profile, optional, if not bundled with b2g desktop.')
    @CommandArgument('--emulator', type=str, default=None,
                     help='Path to android emulator.')
    @CommandArgument('--emulator-arch', type=str, default="x86",
                     help='Emulator arch: x86 or arm.')
    @CommandArgument('test_paths', default=None, nargs='*', metavar='TEST',
                     help='Test to run. Can be specified as a single file, a '
                          'directory, or omitted. If omitted, the entire test suite is '
                          'executed.')
    def run_luciddream_test(self, firefox, b2g_desktop, test_paths, consoles, **params):
        # import luciddream lazily as its marionette dependency make ./mach clobber fails
        # early on TBPL
        import luciddream.runluciddream

        # get_binary_path is going to throw if we haven't built any product
        # but luciddream can still be run if we provide both binaries...
        binary_path=False
        try:
            binary_path = self.get_binary_path()
        except Exception:
            pass

        # otherwise, if we have a build, automatically fetch the binary
        if conditions.is_b2g(self):
            if not b2g_desktop and binary_path:
                b2g_desktop = binary_path
        else:
            if not firefox and binary_path:
                firefox = binary_path

        if not firefox:
            print "Need firefox binary path via --firefox argument"
            return 1
        elif not os.path.exists(firefox):
            print "Firefox binary doesn't exists: " + firefox
            return 1

        if not b2g_desktop:
            print "Need b2g desktop binary path via --b2g-desktop argument"
            return 1
        elif not os.path.exists(b2g_desktop):
            print "B2G desktop binary doesn't exists: " + b2g_desktop
            return 1

        if not test_paths or len(test_paths) == 0:
            print "Please specify a test manifest to run"
            return 1

        browser_args = None
        if consoles:
            browser_args = ["-jsconsole"]
            if "app_args" in params and isinstance(params["app_args"], list):
                params["app_args"].append("-jsconsole")
            else:
                params["app_args"] = ["-jsconsole"]

        for test in test_paths:
          luciddream.runluciddream.main(firefox=firefox, b2g_desktop=b2g_desktop,
            manifest=test, browser_args=browser_args, **params)
