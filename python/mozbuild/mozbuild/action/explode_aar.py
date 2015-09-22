# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import errno
import os
import shutil
import sys
import zipfile

from mozpack.files import FileFinder
import mozpack.path as mozpath
from mozbuild.util import ensureParentDir

def explode(aar, destdir):
    # Take just the support-v4-22.2.1 part.
    name, _ = os.path.splitext(os.path.basename(aar))

    destdir = mozpath.join(destdir, name)
    if os.path.exists(destdir):
        # We always want to start fresh.
        shutil.rmtree(destdir)
    ensureParentDir(destdir)
    with zipfile.ZipFile(aar) as zf:
        zf.extractall(destdir)

    # classes.jar is always present.  However, multiple JAR files with the same
    # name confuses our staged Proguard process in
    # mobile/android/base/Makefile.in, so we make the names unique here.
    classes_jar = mozpath.join(destdir, name + '-classes.jar')
    os.rename(mozpath.join(destdir, 'classes.jar'), classes_jar)

    # Embedded JAR libraries are optional.
    finder = FileFinder(mozpath.join(destdir, 'libs'), find_executables=False)
    for p, _ in finder.find('*.jar'):
        jar = mozpath.join(finder.base, name + '-' + p)
        os.rename(mozpath.join(finder.base, p), jar)

    # Frequently assets/ is present but empty.  Protect against meaningless
    # changes to the AAR files by deleting empty assets/ directories.
    assets = mozpath.join(destdir, 'assets')
    try:
        os.rmdir(assets)
    except OSError, e:
        if e.errno in (errno.ENOTEMPTY, errno.ENOENT):
            pass
        else:
            raise

    return True


def main(argv):
    parser = argparse.ArgumentParser(
        description='Explode Android AAR file.')

    parser.add_argument('--destdir', required=True, help='Destination directory.')
    parser.add_argument('aars', nargs='+', help='Path to AAR file(s).')

    args = parser.parse_args(argv)

    for aar in args.aars:
        if not explode(aar, args.destdir):
            return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
