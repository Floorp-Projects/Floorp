# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This module provides functionality for the command-line build tool
# (mach). It is packaged as a module because everything is a library.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import codecs
import errno
import imp
import logging
import os
import sys
import traceback
import uuid
from collections.abc import Iterable
from typing import Union, Dict, List
from pathlib import Path

from .base import (
    CommandContext,
    MachError,
    MissingFileError,
    NoCommandError,
    UnknownCommandError,
    UnrecognizedArgumentError,
    FailedCommandError,
)
from .config import ConfigSettings
from .dispatcher import CommandAction
from .logging import LoggingManager
from .registrar import Registrar
from .sentry import register_sentry, NoopErrorReporter
from .telemetry import report_invocation_metrics, create_telemetry_from_environment
from .util import setenv, UserError

SUGGEST_MACH_BUSTED_TEMPLATE = r"""
You can invoke |./mach busted| to check if this issue is already on file. If it
isn't, please use |./mach busted file %s| to report it. If |./mach busted| is
misbehaving, you can also inspect the dependencies of bug 1543241.
""".lstrip()

MACH_ERROR_TEMPLATE = (
    r"""
The error occurred in mach itself. This is likely a bug in mach itself or a
fundamental problem with a loaded module.

""".lstrip()
    + SUGGEST_MACH_BUSTED_TEMPLATE
)

ERROR_FOOTER = r"""
If filing a bug, please include the full output of mach, including this error
message.

The details of the failure are as follows:
""".lstrip()

USER_ERROR = r"""
This is a user error and does not appear to be a bug in mach.
""".lstrip()

COMMAND_ERROR_TEMPLATE = (
    r"""
The error occurred in the implementation of the invoked mach command.

This should never occur and is likely a bug in the implementation of that
command.
""".lstrip()
    + SUGGEST_MACH_BUSTED_TEMPLATE
)

MODULE_ERROR_TEMPLATE = (
    r"""
The error occurred in code that was called by the mach command. This is either
a bug in the called code itself or in the way that mach is calling it.
""".lstrip()
    + SUGGEST_MACH_BUSTED_TEMPLATE
)

NO_COMMAND_ERROR = r"""
It looks like you tried to run mach without a command.

Run |mach help| to show a list of commands.
""".lstrip()

UNKNOWN_COMMAND_ERROR = r"""
It looks like you are trying to %s an unknown mach command: %s
%s
Run |mach help| to show a list of commands.
""".lstrip()

SUGGESTED_COMMANDS_MESSAGE = r"""
Did you want to %s any of these commands instead: %s?
"""

UNRECOGNIZED_ARGUMENT_ERROR = r"""
It looks like you passed an unrecognized argument into mach.

The %s command does not accept the arguments: %s
""".lstrip()

INVALID_ENTRY_POINT = r"""
Entry points should return a list of command providers or directories
containing command providers. The following entry point is invalid:

    %s

You are seeing this because there is an error in an external module attempting
to implement a mach command. Please fix the error, or uninstall the module from
your system.
""".lstrip()


class ArgumentParser(argparse.ArgumentParser):
    """Custom implementation argument parser to make things look pretty."""

    def error(self, message):
        """Custom error reporter to give more helpful text on bad commands."""
        if not message.startswith("argument command: invalid choice"):
            argparse.ArgumentParser.error(self, message)
            assert False

        print("Invalid command specified. The list of commands is below.\n")
        self.print_help()
        sys.exit(1)

    def format_help(self):
        text = argparse.ArgumentParser.format_help(self)

        # Strip out the silly command list that would preceed the pretty list.
        #
        # Commands:
        #   {foo,bar}
        #     foo  Do foo.
        #     bar  Do bar.
        search = "Commands:\n  {"
        start = text.find(search)

        if start != -1:
            end = text.find("}\n", start)
            assert end != -1

            real_start = start + len("Commands:\n")
            real_end = end + len("}\n")

            text = text[0:real_start] + text[real_end:]

        return text


class ContextWrapper(object):
    def __init__(self, context, handler):
        object.__setattr__(self, "_context", context)
        object.__setattr__(self, "_handler", handler)

    def __getattribute__(self, key):
        try:
            return getattr(object.__getattribute__(self, "_context"), key)
        except AttributeError as e:
            try:
                ret = object.__getattribute__(self, "_handler")(key)
            except (AttributeError, TypeError):
                # TypeError is in case the handler comes from old code not
                # taking a key argument.
                raise e
            setattr(self, key, ret)
            return ret

    def __setattr__(self, key, value):
        setattr(object.__getattribute__(self, "_context"), key, value)


class MachCommandReference:
    """A reference to a mach command.

    Holds the metadata for a mach command.
    """

    module: Path

    def __init__(self, module: Union[str, Path]):
        self.module = Path(module)


class Mach(object):
    """Main mach driver type.

    This type is responsible for holding global mach state and dispatching
    a command from arguments.

    The following attributes may be assigned to the instance to influence
    behavior:

        populate_context_handler -- If defined, it must be a callable. The
            callable signature is the following:
                populate_context_handler(key=None)
            It acts as a fallback getter for the mach.base.CommandContext
            instance.
            This allows to augment the context instance with arbitrary data
            for use in command handlers.

        require_conditions -- If True, commands that do not have any condition
            functions applied will be skipped. Defaults to False.

        settings_paths -- A list of files or directories in which to search
            for settings files to load.

    """

    USAGE = """%(prog)s [global arguments] command [command arguments]

mach (German for "do") is the main interface to the Mozilla build system and
common developer tasks.

You tell mach the command you want to perform and it does it for you.

Some common commands are:

    %(prog)s build     Build/compile the source tree.
    %(prog)s help      Show full help, including the list of all commands.

To see more help for a specific command, run:

  %(prog)s help <command>
"""

    def __init__(self, cwd: str):
        assert Path(cwd).is_dir()

        self.cwd = cwd
        self.log_manager = LoggingManager()
        self.logger = logging.getLogger(__name__)
        self.settings = ConfigSettings()
        self.settings_paths = []

        if "MACHRC" in os.environ:
            self.settings_paths.append(os.environ["MACHRC"])

        self.log_manager.register_structured_logger(self.logger)
        self.populate_context_handler = None

    def load_commands_from_directory(self, path: Path):
        """Scan for mach commands from modules in a directory.

        This takes a path to a directory, loads the .py files in it, and
        registers and found mach command providers with this mach instance.
        """
        for f in sorted(path.iterdir()):
            if not f.suffix == ".py" or f.name == "__init__.py":
                continue

            full_path = path / f
            module_name = f"mach.commands.{str(f)[0:-3]}"

            self.load_commands_from_file(full_path, module_name=module_name)

    def load_commands_from_file(self, path: Union[str, Path], module_name=None):
        """Scan for mach commands from a file.

        This takes a path to a file and loads it as a Python module under the
        module name specified. If no name is specified, a random one will be
        chosen.
        """
        if module_name is None:
            # Ensure parent module is present otherwise we'll (likely) get
            # an error due to unknown parent.
            if "mach.commands" not in sys.modules:
                mod = imp.new_module("mach.commands")
                sys.modules["mach.commands"] = mod

            module_name = f"mach.commands.{uuid.uuid4().hex}"

        try:
            imp.load_source(module_name, str(path))
        except IOError as e:
            if e.errno != errno.ENOENT:
                raise

            raise MissingFileError(f"{path} does not exist")

    def load_commands_from_spec(
        self, spec: Dict[str, MachCommandReference], topsrcdir: str, missing_ok=False
    ):
        """Load mach commands based on the given spec.

        Takes a dictionary mapping command names to their metadata.
        """
        modules = set(spec[command].module for command in spec)

        for path in modules:
            try:
                self.load_commands_from_file(topsrcdir / path)
            except MissingFileError:
                if not missing_ok:
                    raise

    def load_commands_from_entry_point(self, group="mach.providers"):
        """Scan installed packages for mach command provider entry points. An
        entry point is a function that returns a list of paths to files or
        directories containing command providers.

        This takes an optional group argument which specifies the entry point
        group to use. If not specified, it defaults to 'mach.providers'.
        """
        try:
            import pkg_resources
        except ImportError:
            print(
                "Could not find setuptools, ignoring command entry points",
                file=sys.stderr,
            )
            return

        for entry in pkg_resources.iter_entry_points(group=group, name=None):
            paths = entry.load()()
            if not isinstance(paths, Iterable):
                print(INVALID_ENTRY_POINT % entry)
                sys.exit(1)

            for path in paths:
                path = Path(path)
                if path.is_file():
                    self.load_commands_from_file(path)
                elif path.is_dir():
                    self.load_commands_from_directory(path)
                else:
                    print(f"command provider '{path}' does not exist")

    def define_category(self, name, title, description, priority=50):
        """Provide a description for a named command category."""

        Registrar.register_category(name, title, description, priority)

    @property
    def require_conditions(self):
        return Registrar.require_conditions

    @require_conditions.setter
    def require_conditions(self, value):
        Registrar.require_conditions = value

    def run(self, argv, stdin=None, stdout=None, stderr=None):
        """Runs mach with arguments provided from the command line.

        Returns the integer exit code that should be used. 0 means success. All
        other values indicate failure.
        """
        sentry = NoopErrorReporter()

        # If no encoding is defined, we default to UTF-8 because without this
        # Python 2.7 will assume the default encoding of ASCII. This will blow
        # up with UnicodeEncodeError as soon as it encounters a non-ASCII
        # character in a unicode instance. We simply install a wrapper around
        # the streams and restore once we have finished.
        stdin = sys.stdin if stdin is None else stdin
        stdout = sys.stdout if stdout is None else stdout
        stderr = sys.stderr if stderr is None else stderr

        orig_stdin = sys.stdin
        orig_stdout = sys.stdout
        orig_stderr = sys.stderr

        sys.stdin = stdin
        sys.stdout = stdout
        sys.stderr = stderr

        orig_env = dict(os.environ)

        try:
            # Load settings as early as possible so things in dispatcher.py
            # can use them.
            for provider in Registrar.settings_providers:
                self.settings.register_provider(provider)

            setting_paths_to_pass = [Path(path) for path in self.settings_paths]
            self.load_settings(setting_paths_to_pass)

            if sys.version_info < (3, 0):
                if stdin.encoding is None:
                    sys.stdin = codecs.getreader("utf-8")(stdin)

                if stdout.encoding is None:
                    sys.stdout = codecs.getwriter("utf-8")(stdout)

                if stderr.encoding is None:
                    sys.stderr = codecs.getwriter("utf-8")(stderr)

            # Allow invoked processes (which may not have a handle on the
            # original stdout file descriptor) to know if the original stdout
            # is a TTY. This provides a mechanism to allow said processes to
            # enable emitting code codes, for example.
            if os.isatty(orig_stdout.fileno()):
                setenv("MACH_STDOUT_ISATTY", "1")

            return self._run(argv)
        except KeyboardInterrupt:
            print("mach interrupted by signal or user action. Stopping.")
            return 1

        except Exception:
            # _run swallows exceptions in invoked handlers and converts them to
            # a proper exit code. So, the only scenario where we should get an
            # exception here is if _run itself raises. If _run raises, that's a
            # bug in mach (or a loaded command module being silly) and thus
            # should be reported differently.
            self._print_error_header(argv, sys.stdout)
            print(MACH_ERROR_TEMPLATE % "general")

            exc_type, exc_value, exc_tb = sys.exc_info()
            stack = traceback.extract_tb(exc_tb)

            sentry_event_id = sentry.report_exception(exc_value)
            self._print_exception(
                sys.stdout, exc_type, exc_value, stack, sentry_event_id=sentry_event_id
            )

            return 1

        finally:
            os.environ.clear()
            os.environ.update(orig_env)

            sys.stdin = orig_stdin
            sys.stdout = orig_stdout
            sys.stderr = orig_stderr

    def _run(self, argv):
        if self.populate_context_handler:
            topsrcdir = Path(self.populate_context_handler("topdir"))
            sentry = register_sentry(argv, self.settings, topsrcdir)
        else:
            sentry = NoopErrorReporter()

        context = CommandContext(
            cwd=self.cwd,
            settings=self.settings,
            log_manager=self.log_manager,
            commands=Registrar,
        )

        if self.populate_context_handler:
            context = ContextWrapper(context, self.populate_context_handler)

        parser = self.get_argument_parser(context)
        context.global_parser = parser

        if not len(argv):
            # We don't register the usage until here because if it is globally
            # registered, argparse always prints it. This is not desired when
            # running with --help.
            parser.usage = Mach.USAGE
            parser.print_usage()
            return 0

        try:
            args = parser.parse_args(argv)
        except NoCommandError:
            print(NO_COMMAND_ERROR)
            return 1
        except UnknownCommandError as e:
            suggestion_message = (
                SUGGESTED_COMMANDS_MESSAGE % (e.verb, ", ".join(e.suggested_commands))
                if e.suggested_commands
                else ""
            )
            print(UNKNOWN_COMMAND_ERROR % (e.verb, e.command, suggestion_message))
            return 1
        except UnrecognizedArgumentError as e:
            print(UNRECOGNIZED_ARGUMENT_ERROR % (e.command, " ".join(e.arguments)))
            return 1

        if not hasattr(args, "mach_handler"):
            raise MachError("ArgumentParser result missing mach handler info.")

        context.is_interactive = (
            args.is_interactive
            and sys.__stdout__.isatty()
            and sys.__stderr__.isatty()
            and not os.environ.get("MOZ_AUTOMATION", None)
        )
        context.telemetry = create_telemetry_from_environment(self.settings)

        handler = getattr(args, "mach_handler")
        report_invocation_metrics(context.telemetry, handler.name)

        # Add JSON logging to a file if requested.
        if args.logfile:
            self.log_manager.add_json_handler(args.logfile)

        # Up the logging level if requested.
        log_level = logging.INFO
        if args.verbose:
            log_level = logging.DEBUG

        self.log_manager.register_structured_logger(logging.getLogger("mach"))

        write_times = True
        if (
            args.log_no_times
            or "MACH_NO_WRITE_TIMES" in os.environ
            or "MOZ_AUTOMATION" in os.environ
        ):
            write_times = False

        # Always enable terminal logging. The log manager figures out if we are
        # actually in a TTY or are a pipe and does the right thing.
        self.log_manager.add_terminal_logging(
            level=log_level, write_interval=args.log_interval, write_times=write_times
        )

        if args.settings_file:
            # Argument parsing has already happened, so settings that apply
            # to command line handling (e.g alias, defaults) will be ignored.
            self.load_settings([Path(args.settings_file)])

        try:
            return Registrar._run_command_handler(
                handler,
                context,
                debug_command=args.debug_command,
                profile_command=args.profile_command,
                **vars(args.command_args),
            )
        except KeyboardInterrupt as ki:
            raise ki
        except FailedCommandError as e:
            print(e)
            return e.exit_code
        except UserError:
            # We explicitly don't report UserErrors to Sentry.
            exc_type, exc_value, exc_tb = sys.exc_info()
            # The first two frames are us and are never used.
            stack = traceback.extract_tb(exc_tb)[2:]
            self._print_error_header(argv, sys.stdout)
            print(USER_ERROR)
            self._print_exception(sys.stdout, exc_type, exc_value, stack)
            return 1
        except Exception:
            exc_type, exc_value, exc_tb = sys.exc_info()
            sentry_event_id = sentry.report_exception(exc_value)

            # The first two frames are us and are never used.
            stack = traceback.extract_tb(exc_tb)[2:]

            # If we have nothing on the stack, the exception was raised as part
            # of calling the @Command method itself. This likely means a
            # mismatch between @CommandArgument and arguments to the method.
            # e.g. there exists a @CommandArgument without the corresponding
            # argument on the method. We handle that here until the module
            # loader grows the ability to validate better.
            if not len(stack):
                print(COMMAND_ERROR_TEMPLATE % handler.name)
                self._print_exception(
                    sys.stdout,
                    exc_type,
                    exc_value,
                    traceback.extract_tb(exc_tb),
                    sentry_event_id=sentry_event_id,
                )
                return 1

            # Split the frames into those from the module containing the
            # command and everything else.
            command_frames = []
            other_frames = []

            initial_file = stack[0][0]

            for frame in stack:
                if frame[0] == initial_file:
                    command_frames.append(frame)
                else:
                    other_frames.append(frame)

            # If the exception was in the module providing the command, it's
            # likely the bug is in the mach command module, not something else.
            # If there are other frames, the bug is likely not the mach
            # command's fault.
            self._print_error_header(argv, sys.stdout)

            if len(other_frames):
                print(MODULE_ERROR_TEMPLATE % handler.name)
            else:
                print(COMMAND_ERROR_TEMPLATE % handler.name)

            self._print_exception(
                sys.stdout, exc_type, exc_value, stack, sentry_event_id=sentry_event_id
            )

            return 1

    def log(self, level, action, params, format_str):
        """Helper method to record a structured log event."""
        self.logger.log(level, format_str, extra={"action": action, "params": params})

    def _print_error_header(self, argv, fh):
        fh.write("Error running mach:\n\n")
        fh.write("    ")
        fh.write(repr(argv))
        fh.write("\n\n")

    def _print_exception(self, fh, exc_type, exc_value, stack, sentry_event_id=None):
        fh.write(ERROR_FOOTER)
        fh.write("\n")

        for l in traceback.format_exception_only(exc_type, exc_value):
            fh.write(l)

        fh.write("\n")
        for l in traceback.format_list(stack):
            fh.write(l)

        if not sentry_event_id:
            return

        fh.write("\nSentry event ID: {}\n".format(sentry_event_id))

    def load_settings(self, paths: List[Path]):
        """Load the specified settings files.

        If a directory is specified, the following basenames will be
        searched for in this order:

            machrc, .machrc
        """
        valid_names = ("machrc", ".machrc")

        def find_in_dir(base: Path):
            if base.is_file():
                return base

            for name in valid_names:
                path = base / name
                if path.is_file():
                    return path

        files = map(find_in_dir, paths)
        files = filter(bool, files)

        self.settings.load_files(list(files))

    def get_argument_parser(self, context):
        """Returns an argument parser for the command-line interface."""

        parser = ArgumentParser(
            add_help=False,
            usage="%(prog)s [global arguments] " "command [command arguments]",
        )

        # WARNING!!! If you add a global argument here, also add it to the
        # global argument handling in the top-level `mach` script.
        # Order is important here as it dictates the order the auto-generated
        # help messages are printed.
        global_group = parser.add_argument_group("Global Arguments")

        global_group.add_argument(
            "-v",
            "--verbose",
            dest="verbose",
            action="store_true",
            default=False,
            help="Print verbose output.",
        )
        global_group.add_argument(
            "-l",
            "--log-file",
            dest="logfile",
            metavar="FILENAME",
            type=argparse.FileType("a"),
            help="Filename to write log data to.",
        )
        global_group.add_argument(
            "--log-interval",
            dest="log_interval",
            action="store_true",
            default=False,
            help="Prefix log line with interval from last message rather "
            "than relative time. Note that this is NOT execution time "
            "if there are parallel operations.",
        )
        global_group.add_argument(
            "--no-interactive",
            dest="is_interactive",
            action="store_false",
            help="Automatically selects the default option on any "
            "interactive prompts. If the output is not a terminal, "
            "then --no-interactive is assumed.",
        )
        suppress_log_by_default = False
        if "INSIDE_EMACS" in os.environ:
            suppress_log_by_default = True
        global_group.add_argument(
            "--log-no-times",
            dest="log_no_times",
            action="store_true",
            default=suppress_log_by_default,
            help="Do not prefix log lines with times. By default, "
            "mach will prefix each output line with the time since "
            "command start.",
        )
        global_group.add_argument(
            "-h",
            "--help",
            dest="help",
            action="store_true",
            default=False,
            help="Show this help message.",
        )
        global_group.add_argument(
            "--debug-command",
            action="store_true",
            help="Start a Python debugger when command is dispatched.",
        )
        global_group.add_argument(
            "--profile-command",
            action="store_true",
            help="Capture a Python profile of the mach process as command is dispatched.",
        )
        global_group.add_argument(
            "--settings",
            dest="settings_file",
            metavar="FILENAME",
            default=None,
            help="Path to settings file.",
        )

        # We need to be last because CommandAction swallows all remaining
        # arguments and argparse parses arguments in the order they were added.
        parser.add_argument(
            "command", action=CommandAction, registrar=Registrar, context=context
        )

        return parser
