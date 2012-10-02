#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# Execute as: /import.py <source-distribution> <target-dir>
# <target-dir> must have 'IMPORT_FILES' in it

import sys
import re
import os
import shutil

def die(msg):
    sys.stderr.write('ERROR:' + msg + '\n')
    sys.exit(1)
    
DISTRO = sys.argv[1]
IMPORT_DIR = sys.argv[2]
FILES = []

f = open("%s/IMPORT_FILES" % IMPORT_DIR)
for l in f:
    l = l.strip()
    l = l.strip("'")
    if l.startswith("#"):
        continue
    if not l:
        continue
    FILES.append(l)

for f in FILES:
    print f
    SOURCE_PATH = "%s/%s"%(DISTRO,f)
    DEST_PATH = "%s/%s"%(IMPORT_DIR,f)
    if not os.path.exists(SOURCE_PATH):
        die("%s does not exist"%SOURCE_PATH)
    if not os.path.exists(os.path.dirname(DEST_PATH)):
        os.makedirs(os.path.dirname(DEST_PATH))
    shutil.copyfile(SOURCE_PATH, DEST_PATH)
