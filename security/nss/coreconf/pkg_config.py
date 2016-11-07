#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import subprocess
import sys

def main():
    is_raw = sys.argv[1] == 'raw'
    stdout = None if is_raw else subprocess.PIPE

    if sys.argv[2] == '--libs':
        part_prefix = '-L'
    elif sys.argv[2] == '--cflags':
        part_prefix = '-I'
    else:
        raise Exception('Specify either --libs or --cflags as the second argument.')

    try:
        process = subprocess.Popen(['pkg-config'] + sys.argv[2:], stdout=stdout, stderr=open(os.devnull, 'wb'))

    except OSError:
        # pkg-config is probably not installed
        return

    if is_raw:
        process.wait()
        return

    for part in process.communicate()[0].strip().split():
        if part.startswith(part_prefix):
            print os.path.realpath(os.path.join(part[2:], sys.argv[1]))
            return

if __name__ == '__main__':
    main()
