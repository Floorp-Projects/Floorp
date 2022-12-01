# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import sys
from pathlib import Path

import buildconfig
from mozbuild.repackaging.application_ini import get_application_ini_value
from mozpack import dmg


def main(args):
    parser = argparse.ArgumentParser(
        description="Explode a DMG into its relevant files"
    )

    parser.add_argument("--dsstore", help="DSStore file from")
    parser.add_argument("--background", help="Background file from")
    parser.add_argument("--icon", help="Icon file from")
    parser.add_argument("--volume-name", help="Disk image volume name")

    parser.add_argument("inpath", metavar="PATH_IN", help="Location of files to pack")
    parser.add_argument("dmgfile", metavar="DMG_OUT", help="DMG File to create")

    options = parser.parse_args(args)

    extra_files = []
    if options.dsstore:
        extra_files.append((options.dsstore, ".DS_Store"))
    if options.background:
        extra_files.append((options.background, ".background/background.png"))
    if options.icon:
        extra_files.append((options.icon, ".VolumeIcon.icns"))

    if options.volume_name:
        volume_name = options.volume_name
    else:
        volume_name = get_application_ini_value(
            options.inpath, "App", "CodeName", fallback="Name"
        )

    # Resolve required tools
    dmg_tool = buildconfig.substs["DMG_TOOL"]
    hfs_tool = buildconfig.substs["HFS_TOOL"]
    mkfshfs_tool = buildconfig.substs["MKFSHFS"]

    dmg.create_dmg(
        source_directory=Path(options.inpath),
        output_dmg=Path(options.dmgfile),
        volume_name=volume_name,
        extra_files=extra_files,
        dmg_tool=Path(dmg_tool),
        hfs_tool=Path(hfs_tool),
        mkfshfs_tool=Path(mkfshfs_tool),
    )

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
