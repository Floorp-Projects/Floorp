#!/usr/bin/env python

import sys
import fileinput
import filemanagement
import p4commands

# Defaults
TABSIZE = 4

extensions = ['.h','.cc','.c','.cpp']

ignore_these = ['my_ignore_header.h']

usage = """
Replaces all TAB characters with %(TABSIZE)d space characters.
In addition, all trailing space characters are removed.
usage: trim directory
""" % vars()

if((len(sys.argv) != 2) and (len(sys.argv) != 3)):
    sys.stderr.write(usage)
    sys.exit(2)

directory = sys.argv[1];
if(not filemanagement.pathexist(directory)):
    sys.stderr.write(usage)
    sys.exit(2)

if((len(sys.argv) == 3) and (sys.argv[2] != '--commit')):
    sys.stderr.write(usage)
    sys.exit(2)

commit = False
if(len(sys.argv) == 3):
    commit = True

files_to_fix = []
for extension in extensions:
    files_to_fix.extend(filemanagement.listallfilesinfolder(directory,\
                                                       extension))

def main():
    if (commit):
        p4commands.checkoutallfiles()
    for path,file_name in files_to_fix:
        full_file_name = path + file_name
        if (not commit):
            print full_file_name + ' will be edited'
            continue
        for line in fileinput.input(full_file_name, inplace=True):
            line = line.replace('\t',' '*TABSIZE);    # replace TABs
            line = line.rstrip(None)  # remove trailing whitespaces
            print line                # modify the file
    if (commit):
        p4commands.revertunchangedfiles()

if __name__ == '__main__':
    main()
