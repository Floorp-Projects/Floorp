#!/usr/bin/env python

import os
import subprocess
import sys

def main():
    if sys.platform == 'win32' or len(sys.argv) < 2:
        print(0)
    else:
        cc = os.environ.get('CC', 'cc')
        try:
            cc_is_arg = sys.argv[1] in subprocess.check_output(
              [cc, '--version'], universal_newlines=True)
        except OSError:
            # We probably just don't have CC/cc.
            cc_is_arg = False
        print(int(cc_is_arg))

if __name__ == '__main__':
    main()

