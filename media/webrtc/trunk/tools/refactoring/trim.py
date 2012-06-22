#!/usr/bin/env python

import sys
import fileinput

# Defaults
TABSIZE = 4

usage = """
Replaces all TAB characters with %(TABSIZE)d space characters.
In addition, all trailing space characters are removed.
usage: trim file ...
file ... : files are changed in place without taking any backup.
""" % vars()

def main():

    if len(sys.argv) == 1:
        sys.stderr.write(usage)
        sys.exit(2)

    # Iterate over the lines of all files listed in sys.argv[1:]
    for line in fileinput.input(sys.argv[1:], inplace=True):
        line = line.replace('\t',' '*TABSIZE);    # replace TABs
        line = line.rstrip(None)  # remove trailing whitespaces
        print line                # modify the file

if __name__ == '__main__':
    main()
