# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import os
import sys

import buildconfig
import mozpack.path as mozpath
from mozpack.archive import create_tar_gz_from_files
from mozpack.files import BaseFile

from mozbuild.generated_sources import get_generated_sources


def main(argv):
    parser = argparse.ArgumentParser(description="Produce archive of generated sources")
    parser.add_argument("outputfile", help="File to write output to")
    args = parser.parse_args(argv)

    objdir_abspath = mozpath.abspath(buildconfig.topobjdir)

    def is_valid_entry(entry):
        if isinstance(entry[1], BaseFile):
            entry_abspath = mozpath.abspath(entry[1].path)
        else:
            entry_abspath = mozpath.abspath(entry[1])
        if not entry_abspath.startswith(objdir_abspath):
            print(
                "Warning: omitting generated source [%s] from archive" % entry_abspath,
                file=sys.stderr,
            )
            return False
        # We allow gtest-related files not to be present when not linking gtest
        # during the compile.
        if (
            "LINK_GTEST_DURING_COMPILE" not in buildconfig.substs
            and "/gtest/" in entry_abspath
            and not os.path.exists(entry_abspath)
        ):
            print(
                "Warning: omitting non-existing file [%s] from archive" % entry_abspath,
                file=sys.stderr,
            )
            return False
        return True

    files = dict(filter(is_valid_entry, get_generated_sources()))
    with open(args.outputfile, "wb") as fh:
        create_tar_gz_from_files(fh, files, compresslevel=5)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
