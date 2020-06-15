# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

""" GDB Python customization auto-loader for libxul """

import re

from os.path import (
    abspath,
    dirname,
    exists
)

# Add the toplevel objdir to the gdb source search path.

# In development builds, $objdir/dist/bin/libxul.so is a symlink to
# $objdir/toolkit/library/build/libxul.so, and the latter path is what gdb uses
# to search for gdb scripts.
#
# For artifact builds, libxul.so will be a regular file in $objdir/dist/bin.
# Look both places.

libxul_dir = dirname(__file__)
objdir = None
for relpath in ("../../..", "../.."):
    objdir = abspath(libxul_dir + "/" + relpath)
    if exists(objdir + "/build/.gdbinit"):
        break
else:
    gdb.write("Warning: Gecko objdir not found\n")

if objdir is not None:
    m = re.search(r'[\w ]+: (.*)', gdb.execute("show dir", False, True))
    if m and objdir not in m.group(1).split(":"):
        gdb.execute("set dir {}:{}".format(objdir, m.group(1)))

    # When running from a random directory, the toplevel Gecko .gdbinit may
    # not have been loaded. Load it now.
    gdb.execute("source -s build/.gdbinit.loader")
