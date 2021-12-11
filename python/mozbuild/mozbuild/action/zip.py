# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script creates a zip file, but will also strip any binaries
# it finds before adding them to the zip.

from __future__ import absolute_import, print_function

from mozpack.files import FileFinder
from mozpack.copier import Jarrer
from mozpack.errors import errors
from mozpack.path import match
from mozbuild.action.util import log_build_task

import argparse
import mozpack.path as mozpath
import sys


def main(args):
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-C",
        metavar="DIR",
        default=".",
        help="Change to given directory before considering " "other paths",
    )
    parser.add_argument("--strip", action="store_true", help="Strip executables")
    parser.add_argument(
        "-x",
        metavar="EXCLUDE",
        default=[],
        action="append",
        help="Exclude files that match the pattern",
    )
    parser.add_argument("zip", help="Path to zip file to write")
    parser.add_argument("input", nargs="+", help="Path to files to add to zip")
    args = parser.parse_args(args)

    jarrer = Jarrer()

    with errors.accumulate():
        finder = FileFinder(args.C, find_executables=args.strip)
        for path in args.input:
            for p, f in finder.find(path):
                if not any([match(p, exclude) for exclude in args.x]):
                    jarrer.add(p, f)
        jarrer.copy(mozpath.join(args.C, args.zip))


if __name__ == "__main__":
    log_build_task(main, sys.argv[1:])
