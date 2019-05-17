# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function

import sys
import subprocess

def make_unzip(package):
    subprocess.check_call(['unzip', package])

def main(args):
    if len(args) != 1:
        print('Usage: make_unzip.py <package>',
              file=sys.stderr)
        return 1
    else:
        make_unzip(args[0])
        return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
