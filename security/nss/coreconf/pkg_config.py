#!/usr/bin/env python

import os
import subprocess
import sys

def main():
    try:
        subprocess.Popen(['pkg-config'] + sys.argv[1:], stderr=open(os.devnull, 'wb')).wait()
    except OSError:
        # pkg-config is probably not installed
        pass

if __name__ == '__main__':
    main()
