# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import sys
from pathlib import Path

from mozpack import dmg

from mozbuild.bootstrap import bootstrap_toolchain


def _path_or_none(input: str):
    if not input:
        return None
    return Path(input)


def main(args):
    parser = argparse.ArgumentParser(
        description="Explode a DMG into its relevant files"
    )

    parser.add_argument("--dsstore", help="DSStore file from")
    parser.add_argument("--background", help="Background file from")
    parser.add_argument("--icon", help="Icon file from")

    parser.add_argument("dmgfile", metavar="DMG_IN", help="DMG File to Unpack")
    parser.add_argument(
        "outpath", metavar="PATH_OUT", help="Location to put unpacked files"
    )

    options = parser.parse_args(args)

    dmg_tool = bootstrap_toolchain("dmg/dmg")
    hfs_tool = bootstrap_toolchain("dmg/hfsplus")

    dmg.extract_dmg(
        dmgfile=Path(options.dmgfile),
        output=Path(options.outpath),
        dmg_tool=Path(dmg_tool),
        hfs_tool=Path(hfs_tool),
        dsstore=_path_or_none(options.dsstore),
        background=_path_or_none(options.background),
        icon=_path_or_none(options.icon),
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
