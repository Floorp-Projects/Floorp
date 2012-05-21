#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


"""
Fix references to source files of the form [LOCpath]
so that they are relative to a given source directory.

Substitute the DOT-generated image map into the document.
"""

import os, sys, re

(srcdir, ) = sys.argv[1:]
srcdir = os.path.realpath(srcdir)

f = re.compile(r'\[LOC(.*?)\]')

def replacer(m):
    file = m.group(1)
    file = os.path.realpath(file)
    if not file.startswith(srcdir):
        raise Exception("File %s doesn't start with %s" % (file, srcdir))

    file = file[len(srcdir) + 1:]
    return file

s = re.compile(r'\[MAP(.*?)\]')

def mapreplace(m):
    file = m.group(1)
    c = open(file).read()
    return c

for line in sys.stdin:
    line = f.sub(replacer, line)
    line = s.sub(mapreplace, line)

    sys.stdout.write(line)
