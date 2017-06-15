import argparse
import os

from mozbuild.base import (
    MachCommandBase
)

from mach.decorators import (
    CommandArgument,
    CommandArgumentGroup,
    CommandProvider,
    Command,
)


@CommandProvider
class RunGeckodriver(MachCommandBase):
    """Run the compiled program."""

    @Command('geckodriver', category='post-build',
        description='Run the geckodriver WebDriver implementation')
    @CommandArgument('--binary', type=str,
        help='Firefox binary (defaults to the local build).')
    @CommandArgument('params', nargs='...',
        help='Command-line arguments to be passed through to the program.')

    @CommandArgumentGroup('debugging')
    @CommandArgument('--debug', action='store_true', group='debugging',
        help='Enable the debugger. Not specifying a --debugger option will result in the default debugger being used.')
    @CommandArgument('--debugger', default=None, type=str, group='debugging',
        help='Name of debugger to use.')
    @CommandArgument('--debugger-args', default=None, metavar='params', type=str,
        group='debugging',
        help='Command-line arguments to pass to the debugger itself; split as the Bourne shell would.')
    def run(self, binary, params, debug, debugger, debugger_args):
        try:
            binpath = self.get_binary_path('geckodriver')
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
            binary = self.get_binary_path('app')

        args.extend(["--binary", binary])

        if debug or debugger or debugger_args:
            if 'INSIDE_EMACS' in os.environ:
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
