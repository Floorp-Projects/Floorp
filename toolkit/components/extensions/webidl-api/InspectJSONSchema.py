# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import sys
import os

# Sanity check (ensure the script has been executed through `mach python`).
try:
    import buildconfig

    # try to access an existing property to please flake8 linting and as an
    # additional sanity check.
    buildconfig.topsrcdir
except ModuleNotFoundError or AttributeError:
    print(
        "This script should be executed using `mach python %s`" % __file__,
        file=sys.stderr,
    )
    sys.exit(1)

# Add the current directory to the module path (needed to be able to load
# GenerateWebIDLBindings without having to make it recognized as a python package).
module_dir = os.path.dirname(__file__)
sys.path.append(module_dir)

from GenerateWebIDLBindings import (
    load_and_parse_JSONSchema,
    set_logging_level,
    APIEntry,
)


def get_args_and_argparser():
    parser = argparse.ArgumentParser()

    # global cli flags shared by all sub-commands.
    parser.add_argument("--verbose", "-v", action="count", default=0)
    parser.add_argument(
        "--diff-command",
        type=str,
        metavar="DIFFCMD",
        help="select the diff command used to generate diffs (defaults to 'diff')",
    )

    # --dump-namespaces-list flag (this is also the default for the 'inspect' command
    # when no other flag is specified).
    parser.add_argument(
        "--dump-namespaces-list",
        action="store_true",
        help="'inspect' command flag - dump list of all API namespaces defined in all"
        + " JSONSchema files loaded",
    )

    # --dump-platform-diffs flag and other sub-flags that can be used with it.
    parser.add_argument(
        "--dump-platform-diffs",
        action="store_true",
        help="'inspect' command flag - list all APIs with platform specific differences",
    )
    parser.add_argument(
        "--only-if-webidl-diffs",
        action="store_true",
        help="'inspect' command flag - limits --dump-platform-diff to APIs with differences"
        + " in the generated webidl",
    )

    # --dump-namespaces-info flag and other flags that can be used with it.
    parser.add_argument(
        "--dump-namespaces-info",
        nargs="+",
        type=str,
        metavar="NAMESPACE",
        help="'inspect' command flag - dump data loaded for the given NAMESPACE(s)",
    )
    parser.add_argument(
        "--only-in-schema-group",
        type=str,
        metavar="SCHEMAGROUP",
        help="'inspect' command flag - list api namespace in the given schema group"
        + " (toolkit, browser or mobile)",
    )

    args = parser.parse_args()

    return [args, parser]


# Run the 'inspect' subcommand: these command (and its cli flags) is useful to
# inspect the JSONSchema data loaded, which is explicitly useful when debugging
# or evaluating changes to this scripts (e.g. changes that may be needed if the
# API namespace definition isn't complete or its generated content has issues).
def run_inspect_command(args, schemas, parser):
    # --dump-namespaces-info: print a summary view of all the namespaces available
    # after loading and parsing all the collected JSON schema files.
    if args.dump_namespaces_info:
        if "ALL" in args.dump_namespaces_info:
            for namespace in schemas.get_all_namespace_names():
                schemas.get_namespace(namespace).dump(args.only_in_schema_group)

            return

        for namespace in args.dump_namespaces_info:
            schemas.get_namespace(namespace).dump(args.only_in_schema_group)
        return

    # --dump-platform-diffs: print diffs for the JSON schema where we detected
    # differences between the desktop and mobile JSON schema files.
    if args.dump_platform_diffs:
        for entry in APIEntry.in_multiple_groups:
            entry.dump_platform_diff(args.diff_command, args.only_if_webidl_diffs)
        return

    # Dump the list of all known API namespaces based on all the loaded JSONSchema data.
    if args.dump_namespaces_list:
        schemas.dump_namespaces()
        return

    # Print the help message and exit 1 as a fallback.
    print(
        "ERROR: No option selected, choose one from the following usage message.\n",
        file=sys.stderr,
    )
    parser.print_help()
    sys.exit(1)


def main():
    """Entry point function for this script"""

    [args, parser] = get_args_and_argparser()
    set_logging_level(args.verbose)
    schemas = load_and_parse_JSONSchema()
    run_inspect_command(args, schemas, parser)


if __name__ == "__main__":
    main()
else:
    print(
        "%s is only meant to be loaded as a script using `mach python %s`"
        % (__file__, __file__)
    )
    sys.exit(1)
