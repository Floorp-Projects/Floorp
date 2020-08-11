# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import sys
import os
from mozpack.packager.unpack import unpack
import buildconfig


def main():
    parser = argparse.ArgumentParser(
        description='Unpack a Gecko-based application')
    parser.add_argument('directory', help='Location of the application')
    parser.add_argument('--omnijar', help='Name of the omnijar')

    options = parser.parse_args(sys.argv[1:])

    buildconfig.substs['USE_ELF_HACK'] = False
    buildconfig.substs['PKG_SKIP_STRIP'] = True
    unpack(options.directory, options.omnijar)

if __name__ == "__main__":
    main()
