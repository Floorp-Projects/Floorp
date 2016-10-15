#!/usr/bin/env python

import os
import subprocess

def main():
    cc = os.environ.get('CC', 'cc')
    cc_is_clang = 'clang' in subprocess.check_output([cc, '--version'])
    print int(cc_is_clang)

if __name__ == '__main__':
    main()
