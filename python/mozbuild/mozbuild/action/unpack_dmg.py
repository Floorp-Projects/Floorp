# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function

from mozpack import dmg

import argparse
import sys


def main(args):
    parser = argparse.ArgumentParser(
        description='Explode a DMG into its relevant files')

    parser.add_argument('--dsstore', help='DSStore file from')
    parser.add_argument('--background', help='Background file from')
    parser.add_argument('--icon', help='Icon file from')

    parser.add_argument('dmgfile', metavar='DMG_IN',
                        help='DMG File to Unpack')
    parser.add_argument('outpath', metavar='PATH_OUT',
                        help='Location to put unpacked files')

    options = parser.parse_args(args)

    dmg.extract_dmg(dmgfile=options.dmgfile, output=options.outpath,
                    dsstore=options.dsstore, background=options.background,
                    icon=options.icon)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
