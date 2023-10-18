# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""A generic script to add entries to a file
if the entry does not already exist.

Usage: buildlist.py <filename> <entry> [<entry> ...]
"""
import io
import os
import sys

from mozbuild.util import ensureParentDir, lock_file


def addEntriesToListFile(listFile, entries):
    """Given a file ``listFile`` containing one entry per line,
    add each entry in ``entries`` to the file, unless it is already
    present."""
    ensureParentDir(listFile)
    lock = lock_file(listFile + ".lck")
    try:
        if os.path.exists(listFile):
            f = io.open(listFile)
            existing = set(x.strip() for x in f.readlines())
            f.close()
        else:
            existing = set()
        for e in entries:
            if e not in existing:
                existing.add(e)
        with io.open(listFile, "w", newline="\n") as f:
            f.write("\n".join(sorted(existing)) + "\n")
    finally:
        del lock  # Explicitly release the lock_file to free it


def main(args):
    if len(args) < 2:
        print("Usage: buildlist.py <list file> <entry> [<entry> ...]", file=sys.stderr)
        return 1

    return addEntriesToListFile(args[0], args[1:])


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
