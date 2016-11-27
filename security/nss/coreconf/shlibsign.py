#!/usr/bin/env python2
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import subprocess
import sys

def main():
    for lib_file in sys.argv[1:]:
        if os.path.isfile(lib_file):
            sign(lib_file)

def sign(lib_file):
    ld_lib_path = os.path.realpath(os.path.join(lib_file, '..'))
    bin_path = os.path.realpath(os.path.join(ld_lib_path, '../bin'))

    env = os.environ.copy()
    if sys.platform == 'win32':
        env['PATH'] = os.pathsep.join((env['PATH'], ld_lib_path))
    else:
        env['LD_LIBRARY_PATH'] = env['DYLD_LIBRARY_PATH'] = ld_lib_path

    dev_null = open(os.devnull, 'wb')
    subprocess.check_call([os.path.join(bin_path, 'shlibsign'), '-v', '-i', lib_file], env=env, stdout=dev_null, stderr=dev_null)

if __name__ == '__main__':
    main()
