# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
import zipfile
from pathlib import Path

import mozfile
import mozpack.path as mozpath

from mozbuild.repackaging.application_ini import get_application_ini_value
from mozbuild.util import ensureParentDir

_BCJ_OPTIONS = {
    "x86": ["--x86"],
    "x86_64": ["--x86"],
    "aarch64": [],
    # macOS Universal Builds
    "macos-x86_64-aarch64": [],
}


def repackage_mar(topsrcdir, package, mar, output, arch=None, mar_channel_id=None):
    if not zipfile.is_zipfile(package) and not tarfile.is_tarfile(package):
        raise Exception("Package file %s is not a valid .zip or .tar file." % package)
    if arch and arch not in _BCJ_OPTIONS:
        raise Exception(
            "Unknown architecture {}, available architectures: {}".format(
                arch, list(_BCJ_OPTIONS.keys())
            )
        )

    ensureParentDir(output)
    tmpdir = tempfile.mkdtemp()
    try:
        if tarfile.is_tarfile(package):
            filelist = mozfile.extract_tarball(package, tmpdir)
        else:
            z = zipfile.ZipFile(package)
            z.extractall(tmpdir)
            filelist = z.namelist()
            z.close()

        toplevel_dirs = set([mozpath.split(f)[0] for f in filelist])
        excluded_stuff = set([" ", ".background", ".DS_Store", ".VolumeIcon.icns"])
        toplevel_dirs = toplevel_dirs - excluded_stuff
        # Make sure the .zip file just contains a directory like 'firefox/' at
        # the top, and find out what it is called.
        if len(toplevel_dirs) != 1:
            raise Exception(
                "Package file is expected to have a single top-level directory"
                "(eg: 'firefox'), not: %s" % toplevel_dirs
            )
        ffxdir = mozpath.join(tmpdir, toplevel_dirs.pop())

        make_full_update = mozpath.join(
            topsrcdir, "tools/update-packaging/make_full_update.sh"
        )

        env = os.environ.copy()
        env["MOZ_PRODUCT_VERSION"] = get_application_ini_value(tmpdir, "App", "Version")
        env["MAR"] = mozpath.normpath(mar)
        if arch:
            env["BCJ_OPTIONS"] = " ".join(_BCJ_OPTIONS[arch])
        if mar_channel_id:
            env["MAR_CHANNEL_ID"] = mar_channel_id
        # The Windows build systems have xz installed but it isn't in the path
        # like it is on Linux and Mac OS X so just use the XZ env var so the mar
        # generation scripts can find it.
        xz_path = mozpath.join(topsrcdir, "xz/xz.exe")
        if os.path.exists(xz_path):
            env["XZ"] = mozpath.normpath(xz_path)

        cmd = [make_full_update, output, ffxdir]
        if sys.platform == "win32":
            # make_full_update.sh is a bash script, and Windows needs to
            # explicitly call out the shell to execute the script from Python.

            mozillabuild = os.environ["MOZILLABUILD"]
            if (Path(mozillabuild) / "msys2").exists():
                cmd.insert(0, mozillabuild + "/msys2/usr/bin/bash.exe")
            else:
                cmd.insert(0, mozillabuild + "/msys/bin/bash.exe")
        subprocess.check_call(cmd, env=env)

    finally:
        shutil.rmtree(tmpdir)
