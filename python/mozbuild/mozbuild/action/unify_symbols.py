# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse

from mozpack.copier import FileCopier
from mozpack.errors import errors
from mozpack.files import FileFinder
from mozpack.unify import UnifiedFinder


class UnifiedSymbolsFinder(UnifiedFinder):
    def unify_file(self, path, file1, file2):
        # We expect none of the files to overlap.
        if not file2:
            return file1
        if not file1:
            return file2
        errors.error(
            "{} is in both {} and {}".format(
                path, self._finder1.base, self._finder2.base
            )
        )


def main():
    parser = argparse.ArgumentParser(
        description="Merge two crashreporter symbols directories."
    )
    parser.add_argument("dir1", help="Directory")
    parser.add_argument("dir2", help="Directory to merge")

    options = parser.parse_args()

    dir1_finder = FileFinder(options.dir1)
    dir2_finder = FileFinder(options.dir2)
    finder = UnifiedSymbolsFinder(dir1_finder, dir2_finder)

    copier = FileCopier()
    with errors.accumulate():
        for p, f in finder:
            copier.add(p, f)

    copier.copy(options.dir1, skip_if_older=False)


if __name__ == "__main__":
    main()
