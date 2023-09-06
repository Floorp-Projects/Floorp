# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import shutil
import tarfile
from pathlib import Path

import mozfile
from mozpack.pkg import create_pkg

from mozbuild.bootstrap import bootstrap_toolchain


def repackage_pkg(infile, output):
    if not tarfile.is_tarfile(infile):
        raise Exception("Input file %s is not a valid tarfile." % infile)

    xar_tool = bootstrap_toolchain("xar/xar")
    if not xar_tool:
        raise Exception("Could not find xar tool.")
    mkbom_tool = bootstrap_toolchain("mkbom/mkbom")
    if not mkbom_tool:
        raise Exception("Could not find mkbom tool.")
    # Note: CPIO isn't standard on all OS's
    cpio_tool = shutil.which("cpio")
    if not cpio_tool:
        raise Exception("Could not find cpio.")

    with mozfile.TemporaryDirectory() as tmpdir:
        mozfile.extract_tarball(infile, tmpdir)

        app_list = list(Path(tmpdir).glob("*.app"))
        if len(app_list) != 1:
            raise Exception(
                "Input file should contain a single .app file. %s found."
                % len(app_list)
            )
        create_pkg(
            source_app=Path(app_list[0]),
            output_pkg=Path(output),
            mkbom_tool=Path(mkbom_tool),
            xar_tool=Path(xar_tool),
            cpio_tool=Path(cpio_tool),
        )
