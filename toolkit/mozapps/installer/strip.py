# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''
Strip all files that can be stripped in the given directory.
'''

import sys
from mozpack.files import FileFinder
from mozpack.copier import FileCopier

def strip(dir):
    copier = FileCopier()
    # The FileFinder will give use ExecutableFile instances for files
    # that can be stripped, and copying ExecutableFiles defaults to
    # stripping them unless buildconfig.substs['PKG_SKIP_STRIP'] is set.
    for p, f in FileFinder(dir):
        copier.add(p, f)
    copier.copy(dir)

if __name__ == '__main__':
    strip(sys.argv[1])
