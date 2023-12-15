# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""A generic script to verify all test files are in the
corresponding .toml file.

Usage: xpccheck.py <directory> [<directory> ...]
"""

import os
import sys
from glob import glob

import manifestparser


def getIniTests(testdir):
    mp = manifestparser.ManifestParser(strict=False)
    mp.read(os.path.join(testdir, "xpcshell.toml"))
    return mp.tests


def verifyDirectory(initests, directory):
    files = glob(os.path.join(os.path.abspath(directory), "test_*"))
    for f in files:
        if not os.path.isfile(f):
            continue

        name = os.path.basename(f)
        if name.endswith(".in"):
            name = name[:-3]

        if not name.endswith(".js"):
            continue

        found = False
        for test in initests:
            if os.path.join(os.path.abspath(directory), name) == test["path"]:
                found = True
                break

        if not found:
            print(
                (
                    "TEST-UNEXPECTED-FAIL | xpccheck | test "
                    "%s is missing from test manifest %s!"
                )
                % (
                    name,
                    os.path.join(directory, "xpcshell.toml"),
                ),
                file=sys.stderr,
            )
            sys.exit(1)


def verifyIniFile(initests, directory):
    files = glob(os.path.join(os.path.abspath(directory), "test_*"))
    for test in initests:
        name = test["path"].split("/")[-1]

        found = False
        for f in files:
            fname = f.split("/")[-1]
            if fname.endswith(".in"):
                fname = ".in".join(fname.split(".in")[:-1])

            if os.path.join(os.path.abspath(directory), fname) == test["path"]:
                found = True
                break

        if not found:
            print(
                (
                    "TEST-UNEXPECTED-FAIL | xpccheck | found "
                    "%s in xpcshell.toml and not in directory '%s'"
                )
                % (
                    name,
                    directory,
                ),
                file=sys.stderr,
            )
            sys.exit(1)


def main(argv):
    if len(argv) < 2:
        print(
            "Usage: xpccheck.py <topsrcdir> <directory> [<directory> ...]",
            file=sys.stderr,
        )
        sys.exit(1)

    for d in argv[1:]:
        # xpcshell-unpack is a copy of xpcshell sibling directory and in the Makefile
        # we copy all files (including xpcshell.toml from the sibling directory.
        if d.endswith("toolkit/mozapps/extensions/test/xpcshell-unpack"):
            continue

        initests = getIniTests(d)
        verifyDirectory(initests, d)
        verifyIniFile(initests, d)


if __name__ == "__main__":
    main(sys.argv[1:])
