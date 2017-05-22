# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function

import shutil
import sys
import subprocess

def extract_exe(package, target):
    subprocess.check_call(['7z', 'x', package, 'core'])
    shutil.move('core', target)

def main(args):
    if len(args) != 2:
        print('Usage: exe_7z_extract.py <package> <target>',
              file=sys.stderr)
        return 1
    else:
        extract_exe(args[0], args[1])
        return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
