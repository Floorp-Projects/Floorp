# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import copy
import os

from mach.decorators import Command, CommandArgument
from mozbuild.base import BuildEnvironmentNotFoundException
from mozbuild.base import MachCommandConditions as conditions

here = os.path.abspath(os.path.dirname(__file__))
EXCLUSION_FILES = [
    os.path.join("tools", "rewriting", "Generated.txt"),
    os.path.join("tools", "rewriting", "ThirdPartyPaths.txt"),
]

EXCLUSION_FILES_OPTIONAL = []
thunderbird_excludes = os.path.join("comm", "tools", "lint", "GlobalExclude.txt")
if os.path.exists(thunderbird_excludes):
    EXCLUSION_FILES_OPTIONAL.append(thunderbird_excludes)

GLOBAL_EXCLUDES = ["**/node_modules", "tools/lint/test/files", ".hg", ".git"]

VALID_FORMATTERS = {"black", "clang-format", "rustfmt"}
VALID_ANDROID_FORMATTERS = {"android-format"}

# Code-review bot must index issues from the whole codebase when pushing
# to autoland or try repositories. In such cases, output warnings in the
# task's JSON artifact but do not fail if only warnings are found.
REPORT_WARNINGS = os.environ.get("GECKO_HEAD_REPOSITORY", "").rstrip("/") in (
    "https://hg.mozilla.org/mozilla-central",
    "https://hg.mozilla.org/integration/autoland",
    "https://hg.mozilla.org/try",
)


def setup_argument_parser():
    from mozlint import cli

    return cli.MozlintParser()


def get_global_excludes(**lintargs):
    # exclude misc paths
    excludes = GLOBAL_EXCLUDES[:]
    topsrcdir = lintargs["root"]

    # exclude top level paths that look like objdirs
    excludes.extend(
        [
            name
            for name in os.listdir(topsrcdir)
            if name.startswith("obj") and os.path.isdir(name)
        ]
    )

    if lintargs.get("include_third-party"):
        # For some linters, we want to include the thirdparty code too.
        # Example: trojan-source linter should run also on third party code.
        return excludes

    for path in EXCLUSION_FILES + EXCLUSION_FILES_OPTIONAL:
        with open(os.path.join(topsrcdir, path), "r") as fh:
            excludes.extend([f.strip() for f in fh.readlines()])

    return excludes


@Command(
    "lint",
    category="devenv",
    description="Run linters.",
    parser=setup_argument_parser,
    virtualenv_name="lint",
)
def lint(command_context, *runargs, **lintargs):
    """Run linters."""
    command_context.activate_virtualenv()
    from mozlint import cli, parser

    try:
        buildargs = {}
        buildargs["substs"] = copy.deepcopy(dict(command_context.substs))
        buildargs["defines"] = copy.deepcopy(dict(command_context.defines))
        buildargs["topobjdir"] = command_context.topobjdir
        lintargs.update(buildargs)
    except BuildEnvironmentNotFoundException:
        pass

    lintargs.setdefault("root", command_context.topsrcdir)
    lintargs["exclude"] = get_global_excludes(**lintargs)
    lintargs["config_paths"].insert(0, here)
    lintargs["virtualenv_bin_path"] = command_context.virtualenv_manager.bin_path
    lintargs["virtualenv_manager"] = command_context.virtualenv_manager
    if REPORT_WARNINGS and lintargs.get("show_warnings") is None:
        lintargs["show_warnings"] = "soft"
    for path in EXCLUSION_FILES:
        parser.GLOBAL_SUPPORT_FILES.append(
            os.path.join(command_context.topsrcdir, path)
        )
    setupargs = {
        "mach_command_context": command_context,
    }
    return cli.run(*runargs, setupargs=setupargs, **lintargs)


@Command(
    "eslint",
    category="devenv",
    description="Run eslint or help configure eslint for optimal development.",
)
@CommandArgument(
    "paths",
    default=None,
    nargs="*",
    help="Paths to file or directories to lint, like "
    "'browser/' Defaults to the "
    "current directory if not given.",
)
@CommandArgument(
    "-s",
    "--setup",
    default=False,
    action="store_true",
    help="Configure eslint for optimal development.",
)
@CommandArgument("-b", "--binary", default=None, help="Path to eslint binary.")
@CommandArgument(
    "--fix",
    default=False,
    action="store_true",
    help="Request that eslint automatically fix errors, where possible.",
)
@CommandArgument(
    "--rule",
    default=[],
    dest="rules",
    action="append",
    help="Specify an additional rule for ESLint to run, e.g. 'no-new-object: error'",
)
@CommandArgument(
    "extra_args",
    nargs=argparse.REMAINDER,
    help="Extra args that will be forwarded to eslint.",
)
def eslint(command_context, paths, extra_args=[], **kwargs):
    command_context._mach_context.commands.dispatch(
        "lint",
        command_context._mach_context,
        linters=["eslint"],
        paths=paths,
        argv=extra_args,
        **kwargs
    )


@Command(
    "format",
    category="devenv",
    description="Format files, alternative to 'lint --fix' ",
    parser=setup_argument_parser,
)
def format_files(command_context, paths, extra_args=[], **kwargs):
    linters = kwargs["linters"]

    formatters = VALID_FORMATTERS
    if conditions.is_android(command_context):
        formatters |= VALID_ANDROID_FORMATTERS

    if not linters:
        linters = formatters
    else:
        invalid_linters = set(linters) - formatters
        if invalid_linters:
            print(
                "error: One or more linters passed are not valid formatters. "
                "Note that only the following linters are valid formatters:"
            )
            print("\n".join(sorted(formatters)))
            return 1

    kwargs["linters"] = list(linters)

    kwargs["fix"] = True
    command_context._mach_context.commands.dispatch(
        "lint", command_context._mach_context, paths=paths, argv=extra_args, **kwargs
    )
