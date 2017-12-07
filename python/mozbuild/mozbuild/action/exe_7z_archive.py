# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function

import os
import shutil
import sys
import subprocess
import tempfile
import mozpack.path as mozpath
import buildconfig
from mozbuild.base import BuildEnvironmentNotFoundException

def archive_exe(pkg_dir, tagfile, sfx_package, package):
    tmpdir = tempfile.mkdtemp(prefix='tmp')
    try:
        if pkg_dir:
            shutil.move(pkg_dir, 'core')
        subprocess.check_call(['upx', '--best', '-o', mozpath.join(tmpdir, '7zSD.sfx'), sfx_package])

        try:
            sevenz = buildconfig.config.substs['7Z']
        except BuildEnvironmentNotFoundException:
            # configure hasn't been run, just use the default
            sevenz = '7z'
        subprocess.check_call([sevenz, 'a', '-r', '-t7z', mozpath.join(tmpdir, 'app.7z'), '-mx', '-m0=BCJ2', '-m1=LZMA:d25', '-m2=LZMA:d19', '-m3=LZMA:d19', '-mb0:1', '-mb0s1:2', '-mb0s2:3'])

        with open(package, 'wb') as o:
            for i in [mozpath.join(tmpdir, '7zSD.sfx'), tagfile, mozpath.join(tmpdir, 'app.7z')]:
                shutil.copyfileobj(open(i, 'rb'), o)
        os.chmod(package, 0o0755)
    finally:
        if pkg_dir:
            shutil.move('core', pkg_dir)
        shutil.rmtree(tmpdir)

def main(args):
    if len(args) != 4:
        print('Usage: exe_7z_archive.py <pkg_dir> <tagfile> <sfx_package> <package>',
              file=sys.stderr)
        return 1
    else:
        archive_exe(args[0], args[1], args[2], args[3])
        return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
