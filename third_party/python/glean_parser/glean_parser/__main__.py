# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""Console script for glean_parser."""

import io
from pathlib import Path
import sys

import click


import glean_parser


from . import coverage as mod_coverage
from . import lint
from . import translate as mod_translate
from . import validate_ping


@click.command()
@click.argument(
    "input",
    type=click.Path(exists=False, dir_okay=False, file_okay=True, readable=True),
    nargs=-1,
)
@click.option(
    "--output",
    "-o",
    type=click.Path(dir_okay=True, file_okay=False, writable=True),
    nargs=1,
    required=True,
)
@click.option(
    "--format",
    "-f",
    type=click.Choice(list(mod_translate.OUTPUTTERS.keys())),
    required=True,
)
@click.option(
    "--option",
    "-s",
    help="backend-specific option. Must be of the form key=value",
    type=str,
    multiple=True,
    required=False,
)
@click.option(
    "--allow-reserved",
    is_flag=True,
    help=(
        "If provided, allow the use of reserved fields. "
        "Should only be set when building the Glean library itself."
    ),
)
@click.option(
    "--allow-missing-files",
    is_flag=True,
    help=("Do not treat missing input files as an error."),
)
def translate(input, format, output, option, allow_reserved, allow_missing_files):
    """
    Translate metrics.yaml and pings.yaml files to other formats.
    """
    option_dict = {}
    for opt in option:
        key, val = opt.split("=", 1)
        option_dict[key] = val

    sys.exit(
        mod_translate.translate(
            [Path(x) for x in input],
            format,
            Path(output),
            option_dict,
            {
                "allow_reserved": allow_reserved,
                "allow_missing_files": allow_missing_files,
            },
        )
    )


@click.command()
@click.option(
    "--schema",
    "-s",
    type=str,
    nargs=1,
    required=True,
    help=("HTTP url or file path to Glean ping schema. If remote, will cache to disk."),
)
def check(schema):
    """
    Validate the contents of a Glean ping.

    The ping contents are read from stdin, and the validation errors are
    written to stdout.
    """
    sys.exit(
        validate_ping.validate_ping(
            io.TextIOWrapper(sys.stdin.buffer, encoding="utf-8"),
            io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8"),
            schema_url=schema,
        )
    )


@click.command()
@click.argument(
    "input",
    type=click.Path(exists=True, dir_okay=False, file_okay=True, readable=True),
    nargs=-1,
)
@click.option(
    "--allow-reserved",
    is_flag=True,
    help=(
        "If provided, allow the use of reserved fields. "
        "Should only be set when building the Glean library itself."
    ),
)
@click.option(
    "--allow-missing-files",
    is_flag=True,
    help=("Do not treat missing input files as an error."),
)
def glinter(input, allow_reserved, allow_missing_files):
    """
    Runs a linter over the metrics.
    """
    sys.exit(
        lint.glinter(
            [Path(x) for x in input],
            {
                "allow_reserved": allow_reserved,
                "allow_missing_files": allow_missing_files,
            },
        )
    )


@click.command()
@click.option(
    "-c",
    "--coverage_file",
    type=click.Path(exists=True, dir_okay=False, file_okay=True, readable=True),
    required=True,
    multiple=True,
)
@click.argument(
    "metrics_files",
    type=click.Path(exists=True, dir_okay=False, file_okay=True, readable=True),
    nargs=-1,
)
@click.option(
    "-o",
    "--output",
    type=click.Path(exists=False, dir_okay=False, file_okay=True, writable=True),
    required=True,
)
@click.option(
    "--format",
    "-f",
    type=click.Choice(list(mod_coverage.OUTPUTTERS.keys())),
    required=True,
)
@click.option(
    "--allow-reserved",
    is_flag=True,
    help=(
        "If provided, allow the use of reserved fields. "
        "Should only be set when building the Glean library itself."
    ),
)
def coverage(coverage_file, metrics_files, format, output, allow_reserved):
    """
    Produce a coverage analysis file given raw coverage output and a set of
    metrics.yaml files.
    """
    sys.exit(
        mod_coverage.coverage(
            [Path(x) for x in coverage_file],
            [Path(x) for x in metrics_files],
            format,
            Path(output),
            {
                "allow_reserved": allow_reserved,
            },
        )
    )


@click.group()
@click.version_option(glean_parser.__version__, prog_name="glean_parser")
def main(args=None):
    """Command line utility for glean_parser."""
    pass


main.add_command(translate)
main.add_command(check)
main.add_command(glinter)
main.add_command(coverage)


def main_wrapper(args=None):
    """
    A simple wrapper around click's `main` to display the glean_parser version
    when there is an error.
    """
    try:
        main(args=args)
    except SystemExit as e:
        if e.code != 0:
            print(f"ERROR running glean_parser v{glean_parser.__version__}")
        raise


if __name__ == "__main__":
    main_wrapper()  # pragma: no cover
