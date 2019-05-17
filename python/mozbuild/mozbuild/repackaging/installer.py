# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import tempfile
import shutil
import zipfile
import mozpack.path as mozpath
from mozbuild.action.exe_7z_archive import archive_exe
from mozbuild.util import ensureParentDir


def repackage_installer(topsrcdir, tag, setupexe, package, output, package_name, sfx_stub, use_upx):
    if package and not zipfile.is_zipfile(package):
        raise Exception("Package file %s is not a valid .zip file." % package)
    if package is not None and package_name is None:
        raise Exception("Package name must be provided, if a package is provided.")
    if package is None and package_name is not None:
        raise Exception("Package name must not be provided, if a package is not provided.")

    # We need the full path for the tag and output, since we chdir later.
    tag = mozpath.realpath(tag)
    output = mozpath.realpath(output)
    ensureParentDir(output)

    tmpdir = tempfile.mkdtemp()
    old_cwd = os.getcwd()
    try:
        if package:
            z = zipfile.ZipFile(package)
            z.extractall(tmpdir)
            z.close()

        # Copy setup.exe into the root of the install dir, alongside the
        # package.
        shutil.copyfile(setupexe, mozpath.join(tmpdir, mozpath.basename(setupexe)))

        # archive_exe requires us to be in the directory where the package is
        # unpacked (the tmpdir)
        os.chdir(tmpdir)

        sfx_package = mozpath.join(topsrcdir, sfx_stub)

        archive_exe(package_name, tag, sfx_package, output, use_upx)

    finally:
        os.chdir(old_cwd)
        shutil.rmtree(tmpdir)
