# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import os

import buildconfig
import mozpack.path as mozpath
from mozpack.copier import FileCopier
from mozpack.errors import errors
from mozpack.files import FileFinder
from mozpack.unify import UnifiedFinder


class UnifiedTestFinder(UnifiedFinder):
    def unify_file(self, path, file1, file2):
        unified = super(UnifiedTestFinder, self).unify_file(path, file1, file2)
        basename = mozpath.basename(path)
        if basename == "mozinfo.json":
            # The mozinfo.json files contain processor info, which differs
            # between both ends.
            # Remove the block when this assert is hit.
            assert not unified
            errors.ignore_errors()
            self._report_difference(path, file1, file2)
            errors.ignore_errors(False)
            return file1
        elif basename == "dump_syms_mac":
            # At the moment, the dump_syms_mac executable is a x86_64 binary
            # on both ends. We can't create a universal executable from twice
            # the same executable.
            # When this assert hits, remove this block.
            assert file1.open().read() == file2.open().read()
            return file1
        return unified


def main():
    parser = argparse.ArgumentParser(
        description="Merge two directories, creating Universal binaries for "
        "executables and libraries they contain."
    )
    parser.add_argument("dir1", help="Directory")
    parser.add_argument("dir2", help="Directory to merge")

    options = parser.parse_args()

    buildconfig.substs["OS_ARCH"] = "Darwin"
    buildconfig.substs["LIPO"] = os.environ.get("LIPO")

    dir1_finder = FileFinder(options.dir1, find_executables=True, find_dotfiles=True)
    dir2_finder = FileFinder(options.dir2, find_executables=True, find_dotfiles=True)
    finder = UnifiedTestFinder(dir1_finder, dir2_finder)

    copier = FileCopier()
    with errors.accumulate():
        for p, f in finder:
            copier.add(p, f)

    copier.copy(options.dir1, skip_if_older=False)


if __name__ == "__main__":
    main()
