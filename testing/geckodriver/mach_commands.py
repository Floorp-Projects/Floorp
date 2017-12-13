# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import os

from mach.decorators import (
    Command,
    CommandArgument,
    CommandArgumentGroup,
    CommandProvider,
    SubCommand,
)

from mozbuild.base import (
    MachCommandBase,
    MachCommandConditions as conditions,
)


@CommandProvider
class GeckoDriver(MachCommandBase):

    @Command("geckodriver",
        category="post-build",
        description="WebDriver implementation for Gecko",
        conditions=[conditions.is_firefox])
    def geckodriver(self):
        pass

    @SubCommand("geckodriver", "run",
        description="Run geckodriver.")
    @CommandArgument("--binary", type=str,
        help="Firefox binary (defaults to the local build).")
    @CommandArgument("params", nargs="...",
        help="Flags to be passed through to geckodriver.")
    @CommandArgumentGroup("debugging")
    @CommandArgument("--debug", action="store_true", group="debugging",
        help="Enable the debugger. Not specifying a --debugger option will result in the default debugger being used.")
    @CommandArgument("--debugger", default=None, type=str, group="debugging",
        help="Name of debugger to use.")
    @CommandArgument("--debugger-args", default=None, metavar="params", type=str,
        group="debugging",
        help="Flags to pass to the debugger itself; split as the Bourne shell would.")
    def run(self, binary, params, debug, debugger, debugger_args):
        try:
            binpath = self.get_binary_path("geckodriver")
        except Exception as e:
                print("It looks like geckodriver isn't built. "
                      "Add ac_add_options --enable-geckodrver to your mozconfig ",
                      "and run |mach build| to build it.")
                print(e)
                return 1

        args = [binpath]

        if params:
            args.extend(params)

        if binary is None:
            binary = self.get_binary_path("app")

        args.extend(["--binary", binary])

        if debug or debugger or debugger_args:
            if "INSIDE_EMACS" in os.environ:
                self.log_manager.terminal_handler.setLevel(logging.WARNING)

            import mozdebug
            if not debugger:
                # No debugger name was provided. Look for the default ones on
                # current OS.
                debugger = mozdebug.get_default_debugger_name(mozdebug.DebuggerSearch.KeepLooking)

            if debugger:
                self.debuggerInfo = mozdebug.get_debugger_info(debugger, debugger_args)
                if not self.debuggerInfo:
                    print("Could not find a suitable debugger in your PATH.")
                    return 1

            # Parameters come from the CLI. We need to convert them before
            # their use.
            if debugger_args:
                from mozbuild import shellutil
                try:
                    debugger_args = shellutil.split(debugger_args)
                except shellutil.MetaCharacterException as e:
                    print("The --debugger-args you passed require a real shell to parse them.")
                    print("(We can't handle the %r character.)" % e.char)
                    return 1

            # Prepend the debugger args.
            args = [self.debuggerInfo.path] + self.debuggerInfo.args + args

        return self.run_process(args=args, ensure_exit_code=False,
            pass_thru=True)

    @SubCommand("geckodriver", "build",
        description="Build geckodriver.")
    @CommandArgument("-v", "--verbose", action="store_true",
        help="Verbose output for what commands the build is running.")
    def build(self, verbose=False):
        from mozbuild.controller.building import BuildDriver

        self.log_manager.enable_all_structured_loggers()

        driver = self._spawn(BuildDriver)
        return driver.build(
            what=["testing/geckodriver"],
            verbose=verbose,
            mach_context=self._mach_context)

    @SubCommand("geckodriver", "test",
        description="Run geckodriver unit tests.")
    @CommandArgument("-v", "--verbose", action="store_true",
        help="Verbose output for what commands the build is running.")
    def test(self, verbose=False):
        from mozbuild.controller.building import BuildDriver

        self.log_manager.enable_all_structured_loggers()

        driver = self._spawn(BuildDriver)
        return driver.build(
            what=["testing/geckodriver/check"],
            verbose=verbose,
            mach_context=self._mach_context)
