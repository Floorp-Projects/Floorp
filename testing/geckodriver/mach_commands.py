# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import logging
import os

from mach.decorators import Command, CommandArgument, CommandArgumentGroup
from mozbuild.base import BinaryNotFoundException


@Command(
    "geckodriver",
    category="post-build",
    description="Run the WebDriver implementation for Gecko.",
)
@CommandArgument(
    "--binary", type=str, help="Firefox binary (defaults to the local build)."
)
@CommandArgument(
    "params", nargs="...", help="Flags to be passed through to geckodriver."
)
@CommandArgumentGroup("debugging")
@CommandArgument(
    "--debug",
    action="store_true",
    group="debugging",
    help="Enable the debugger. Not specifying a --debugger "
    "option will result in the default debugger "
    "being used.",
)
@CommandArgument(
    "--debugger",
    default=None,
    type=str,
    group="debugging",
    help="Name of debugger to use.",
)
@CommandArgument(
    "--debugger-args",
    default=None,
    metavar="params",
    type=str,
    group="debugging",
    help="Flags to pass to the debugger itself; split as the Bourne shell would.",
)
def run(command_context, binary, params, debug, debugger, debugger_args):
    try:
        binpath = command_context.get_binary_path("geckodriver")
    except BinaryNotFoundException as e:
        command_context.log(
            logging.ERROR, "geckodriver", {"error": str(e)}, "ERROR: {error}"
        )
        command_context.log(
            logging.INFO,
            "geckodriver",
            {},
            "It looks like geckodriver isn't built. "
            "Add ac_add_options --enable-geckodriver to your "
            "mozconfig "
            "and run |./mach build| to build it.",
        )
        return 1

    args = [binpath]

    if params:
        args.extend(params)

    if binary is None:
        try:
            binary = command_context.get_binary_path("app")
        except BinaryNotFoundException as e:
            command_context.log(
                logging.ERROR, "geckodriver", {"error": str(e)}, "ERROR: {error}"
            )
            command_context.log(
                logging.INFO, "geckodriver", {"help": e.help()}, "{help}"
            )
            return 1

    args.extend(["--binary", binary])

    if debug or debugger or debugger_args:
        if "INSIDE_EMACS" in os.environ:
            command_context.log_manager.terminal_handler.setLevel(logging.WARNING)

        import mozdebug

        if not debugger:
            # No debugger name was provided. Look for the default ones on
            # current OS.
            debugger = mozdebug.get_default_debugger_name(
                mozdebug.DebuggerSearch.KeepLooking
            )

        if debugger:
            debuggerInfo = mozdebug.get_debugger_info(debugger, debugger_args)
            if not debuggerInfo:
                print("Could not find a suitable debugger in your PATH.")
                return 1

        # Parameters come from the CLI. We need to convert them before
        # their use.
        if debugger_args:
            from mozbuild import shellutil

            try:
                debugger_args = shellutil.split(debugger_args)
            except shellutil.MetaCharacterException as e:
                print(
                    "The --debugger-args you passed require a real shell to parse them."
                )
                print("(We can't handle the %r character.)" % e.char)
                return 1

        # Prepend the debugger args.
        args = [debuggerInfo.path] + debuggerInfo.args + args

    return command_context.run_process(
        args=args, ensure_exit_code=False, pass_thru=True
    )
