# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys
import tempfile
import shutil
import zipfile
import tarfile
import subprocess
import mozpack.path as mozpath
from application_ini import get_application_ini_value
from mozbuild.util import ensureParentDir


def repackage_mar(topsrcdir, package, mar, output):
    if not zipfile.is_zipfile(package) and not tarfile.is_tarfile(package):
        raise Exception("Package file %s is not a valid .zip or .tar file." % package)

    ensureParentDir(output)
    tmpdir = tempfile.mkdtemp()
    try:
        if zipfile.is_zipfile(package):
            z = zipfile.ZipFile(package)
            z.extractall(tmpdir)
            filelist = z.namelist()
            z.close()
        else:
            z = tarfile.open(package)
            z.extractall(tmpdir)
            filelist = z.getnames()
            z.close()

        toplevel_dirs = set([mozpath.split(f)[0] for f in filelist])
        excluded_stuff = set([' ', '.background', '.DS_Store', '.VolumeIcon.icns'])
        toplevel_dirs = toplevel_dirs - excluded_stuff
        # Make sure the .zip file just contains a directory like 'firefox/' at
        # the top, and find out what it is called.
        if len(toplevel_dirs) != 1:
            raise Exception("Package file is expected to have a single top-level directory"
                            "(eg: 'firefox'), not: %s" % toplevel_dirs)
        ffxdir = mozpath.join(tmpdir, toplevel_dirs.pop())

        make_full_update = mozpath.join(topsrcdir, 'tools/update-packaging/make_full_update.sh')

        env = os.environ.copy()
        env['MOZ_FULL_PRODUCT_VERSION'] = get_application_ini_value(tmpdir, 'App', 'Version')
        env['MAR'] = mozpath.normpath(mar)

        cmd = [make_full_update, output, ffxdir]
        if sys.platform == 'win32':
            # make_full_update.sh is a bash script, and Windows needs to
            # explicitly call out the shell to execute the script from Python.
            cmd.insert(0, env['MOZILLABUILD'] + '/msys/bin/bash.exe')
        subprocess.check_call(cmd, env=env)

    finally:
        shutil.rmtree(tmpdir)
