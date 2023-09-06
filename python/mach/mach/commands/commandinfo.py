# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import re
import subprocess
import sys
from itertools import chain
from pathlib import Path

import attr
from mozbuild.util import memoize

from mach.decorators import Command, CommandArgument, SubCommand

COMPLETION_TEMPLATES_DIR = Path(__file__).resolve().parent / "completion_templates"


@attr.s
class CommandInfo(object):
    name = attr.ib(type=str)
    description = attr.ib(type=str)
    subcommands = attr.ib(type=list)
    options = attr.ib(type=dict)
    subcommand = attr.ib(type=str, default=None)


def render_template(shell, context):
    filename = "{}.template".format(shell)
    with open(COMPLETION_TEMPLATES_DIR / filename) as fh:
        template = fh.read()
    return template % context


@memoize
def command_handlers(command_context):
    """A dictionary of command handlers keyed by command name."""
    return command_context._mach_context.commands.command_handlers


@memoize
def commands(command_context):
    """A sorted list of all command names."""
    return sorted(command_handlers(command_context))


def _get_parser_options(parser):
    options = {}
    for action in parser._actions:
        # ignore positional args
        if not action.option_strings:
            continue

        # ignore suppressed args
        if action.help == argparse.SUPPRESS:
            continue

        options[tuple(action.option_strings)] = action.help or ""
    return options


@memoize
def global_options(command_context):
    """Return a dict of global options.

    Of the form `{("-o", "--option"): "description"}`.
    """
    for group in command_context._mach_context.global_parser._action_groups:
        if group.title == "Global Arguments":
            return _get_parser_options(group)


@memoize
def _get_handler_options(handler):
    """Return a dict of options for the given handler.

    Of the form `{("-o", "--option"): "description"}`.
    """
    options = {}
    for option_strings, val in handler.arguments:
        # ignore positional args
        if option_strings[0][0] != "-":
            continue

        options[tuple(option_strings)] = val.get("help", "")

    if handler._parser:
        options.update(_get_parser_options(handler.parser))

    return options


def _get_handler_info(handler):
    try:
        options = _get_handler_options(handler)
    except (Exception, SystemExit):
        # We don't want misbehaving commands to break tab completion,
        # ignore any exceptions.
        options = {}

    subcommands = []
    for sub in sorted(handler.subcommand_handlers):
        subcommands.append(_get_handler_info(handler.subcommand_handlers[sub]))

    return CommandInfo(
        name=handler.name,
        description=handler.description or "",
        options=options,
        subcommands=subcommands,
        subcommand=handler.subcommand,
    )


@memoize
def commands_info(command_context):
    """Return a list of CommandInfo objects for each command."""
    commands_info = []
    # Loop over self.commands() rather than self.command_handlers().items() for
    # alphabetical order.
    for c in commands(command_context):
        commands_info.append(_get_handler_info(command_handlers(command_context)[c]))
    return commands_info


@Command("mach-commands", category="misc", description="List all mach commands.")
def run_commands(command_context):
    print("\n".join(commands(command_context)))


@Command(
    "mach-debug-commands",
    category="misc",
    description="Show info about available mach commands.",
)
@CommandArgument(
    "match",
    metavar="MATCH",
    default=None,
    nargs="?",
    help="Only display commands containing given substring.",
)
def run_debug_commands(command_context, match=None):
    import inspect

    for command, handler in command_handlers(command_context).items():
        if match and match not in command:
            continue

        func = handler.func

        print(command)
        print("=" * len(command))
        print("")
        print("File: %s" % inspect.getsourcefile(func))
        print("Function: %s" % func.__name__)
        print("")


@Command(
    "mach-completion",
    category="misc",
    description="Prints a list of completion strings for the specified command.",
)
@CommandArgument(
    "args", default=None, nargs=argparse.REMAINDER, help="Command to complete."
)
def run_completion(command_context, args):
    if not args:
        print("\n".join(commands(command_context)))
        return

    is_help = "help" in args
    command = None
    for i, arg in enumerate(args):
        if arg in commands(command_context):
            command = arg
            args = args[i + 1 :]
            break

    # If no command is typed yet, just offer the commands.
    if not command:
        print("\n".join(commands(command_context)))
        return

    handler = command_handlers(command_context)[command]
    # If a subcommand was typed, update the handler.
    for arg in args:
        if arg in handler.subcommand_handlers:
            handler = handler.subcommand_handlers[arg]
            break

    targets = sorted(handler.subcommand_handlers.keys())
    if is_help:
        print("\n".join(targets))
        return

    targets.append("help")
    targets.extend(chain(*_get_handler_options(handler).keys()))
    print("\n".join(targets))


def _zsh_describe(value, description=None):
    value = '"' + value.replace(":", "\\:")
    if description:
        description = subprocess.list2cmdline(
            [re.sub(r'(["\'#&;`|*?~<>^()\[\]{}$\\\x0A\xFF])', r"\\\1", description)]
        ).lstrip('"')

        if description.endswith('"') and not description.endswith(r"\""):
            description = description[:-1]

        value += ":{}".format(description)

    value += '"'

    return value


@SubCommand(
    "mach-completion",
    "bash",
    description="Print mach completion script for bash shell",
)
@CommandArgument(
    "-f",
    "--file",
    dest="outfile",
    default=None,
    help="File path to save completion script.",
)
def completion_bash(command_context, outfile):
    commands_subcommands = []
    case_options = []
    case_subcommands = []
    for i, cmd in enumerate(commands_info(command_context)):
        # Build case statement for options.
        options = []
        for opt_strs, description in cmd.options.items():
            for opt in opt_strs:
                options.append(_zsh_describe(opt, None).strip('"'))

        if options:
            case_options.append(
                "\n".join(
                    [
                        "            ({})".format(cmd.name),
                        '            opts="${{opts}} {}"'.format(" ".join(options)),
                        "            ;;",
                        "",
                    ]
                )
            )

        # Build case statement for subcommand options.
        for sub in cmd.subcommands:
            options = []
            for opt_strs, description in sub.options.items():
                for opt in opt_strs:
                    options.append(_zsh_describe(opt, None))

            if options:
                case_options.append(
                    "\n".join(
                        [
                            '            ("{} {}")'.format(sub.name, sub.subcommand),
                            '            opts="${{opts}} {}"'.format(" ".join(options)),
                            "            ;;",
                            "",
                        ]
                    )
                )

        # Build case statement for subcommands.
        subcommands = [_zsh_describe(s.subcommand, None) for s in cmd.subcommands]
        if subcommands:
            commands_subcommands.append(
                '[{}]=" {} "'.format(
                    cmd.name, " ".join([h.subcommand for h in cmd.subcommands])
                )
            )

            case_subcommands.append(
                "\n".join(
                    [
                        "            ({})".format(cmd.name),
                        '            subs="${{subs}} {}"'.format(" ".join(subcommands)),
                        "            ;;",
                        "",
                    ]
                )
            )

    globalopts = [
        opt for opt_strs in global_options(command_context) for opt in opt_strs
    ]
    context = {
        "case_options": "\n".join(case_options),
        "case_subcommands": "\n".join(case_subcommands),
        "commands": " ".join(commands(command_context)),
        "commands_subcommands": " ".join(sorted(commands_subcommands)),
        "globalopts": " ".join(sorted(globalopts)),
    }

    outfile = open(outfile, "w") if outfile else sys.stdout
    print(render_template("bash", context), file=outfile)


@SubCommand(
    "mach-completion",
    "zsh",
    description="Print mach completion script for zsh shell",
)
@CommandArgument(
    "-f",
    "--file",
    dest="outfile",
    default=None,
    help="File path to save completion script.",
)
def completion_zsh(command_context, outfile):
    commands_descriptions = []
    commands_subcommands = []
    case_options = []
    case_subcommands = []
    for i, cmd in enumerate(commands_info(command_context)):
        commands_descriptions.append(_zsh_describe(cmd.name, cmd.description))

        # Build case statement for options.
        options = []
        for opt_strs, description in cmd.options.items():
            for opt in opt_strs:
                options.append(_zsh_describe(opt, description))

        if options:
            case_options.append(
                "\n".join(
                    [
                        "            ({})".format(cmd.name),
                        "            opts+=({})".format(" ".join(options)),
                        "            ;;",
                        "",
                    ]
                )
            )

        # Build case statement for subcommand options.
        for sub in cmd.subcommands:
            options = []
            for opt_strs, description in sub.options.items():
                for opt in opt_strs:
                    options.append(_zsh_describe(opt, description))

            if options:
                case_options.append(
                    "\n".join(
                        [
                            "            ({} {})".format(sub.name, sub.subcommand),
                            "            opts+=({})".format(" ".join(options)),
                            "            ;;",
                            "",
                        ]
                    )
                )

        # Build case statement for subcommands.
        subcommands = [
            _zsh_describe(s.subcommand, s.description) for s in cmd.subcommands
        ]
        if subcommands:
            commands_subcommands.append(
                '[{}]=" {} "'.format(
                    cmd.name, " ".join([h.subcommand for h in cmd.subcommands])
                )
            )

            case_subcommands.append(
                "\n".join(
                    [
                        "            ({})".format(cmd.name),
                        "            subs+=({})".format(" ".join(subcommands)),
                        "            ;;",
                        "",
                    ]
                )
            )

    globalopts = []
    for opt_strings, description in global_options(command_context).items():
        for opt in opt_strings:
            globalopts.append(_zsh_describe(opt, description))

    context = {
        "case_options": "\n".join(case_options),
        "case_subcommands": "\n".join(case_subcommands),
        "commands": " ".join(sorted(commands_descriptions)),
        "commands_subcommands": " ".join(sorted(commands_subcommands)),
        "globalopts": " ".join(sorted(globalopts)),
    }

    outfile = open(outfile, "w") if outfile else sys.stdout
    print(render_template("zsh", context), file=outfile)


@SubCommand(
    "mach-completion",
    "fish",
    description="Print mach completion script for fish shell",
)
@CommandArgument(
    "-f",
    "--file",
    dest="outfile",
    default=None,
    help="File path to save completion script.",
)
def completion_fish(command_context, outfile):
    def _append_opt_strs(comp, opt_strs):
        for opt in opt_strs:
            if opt.startswith("--"):
                comp += " -l {}".format(opt[2:])
            elif opt.startswith("-"):
                comp += " -s {}".format(opt[1:])
        return comp

    globalopts = []
    for opt_strs, description in global_options(command_context).items():
        comp = (
            "complete -c mach -n '__fish_mach_complete_no_command' "
            "-d '{}'".format(description.replace("'", "\\'"))
        )
        comp = _append_opt_strs(comp, opt_strs)
        globalopts.append(comp)

    cmds = []
    cmds_opts = []
    for i, cmd in enumerate(commands_info(command_context)):
        cmds.append(
            "complete -c mach -f -n '__fish_mach_complete_no_command' "
            "-a {} -d '{}'".format(cmd.name, cmd.description.replace("'", "\\'"))
        )

        cmds_opts += ["# {}".format(cmd.name)]

        subcommands = " ".join([s.subcommand for s in cmd.subcommands])
        for opt_strs, description in cmd.options.items():
            comp = (
                "complete -c mach -A -n '__fish_mach_complete_command {} {}' "
                "-d '{}'".format(cmd.name, subcommands, description.replace("'", "\\'"))
            )
            comp = _append_opt_strs(comp, opt_strs)
            cmds_opts.append(comp)

        for sub in cmd.subcommands:
            for opt_strs, description in sub.options.items():
                comp = (
                    "complete -c mach -A -n '__fish_mach_complete_subcommand {} {}' "
                    "-d '{}'".format(
                        sub.name, sub.subcommand, description.replace("'", "\\'")
                    )
                )
                comp = _append_opt_strs(comp, opt_strs)
                cmds_opts.append(comp)

            description = sub.description or ""
            description = description.replace("'", "\\'")
            comp = (
                "complete -c mach -A -n '__fish_mach_complete_command {} {}' "
                "-d '{}' -a {}".format(
                    cmd.name, subcommands, description, sub.subcommand
                )
            )
            cmds_opts.append(comp)

        if i < len(commands(command_context)) - 1:
            cmds_opts.append("")

    context = {
        "commands": " ".join(commands(command_context)),
        "command_completions": "\n".join(cmds),
        "command_option_completions": "\n".join(cmds_opts),
        "global_option_completions": "\n".join(globalopts),
    }

    outfile = open(outfile, "w") if outfile else sys.stdout
    print(render_template("fish", context), file=outfile)
