#!/usr/bin/env python

import os
import subprocess

def main():
    try:
        for part in subprocess.Popen(['pkg-config', '--cflags', 'nspr'], stdout=subprocess.PIPE, stderr=open(os.devnull, 'wb')).communicate()[0].strip().split():
            if part.startswith('-I'):
                print part[2:]
                return
    except OSError:
        # pkg-config is probably not installed
        pass

if __name__ == '__main__':
    main()
