#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script processes NSS .def files according to the rules defined in
# a comment at the top of each one. The files are used to define the
# exports from NSS shared libraries, with -DEFFILE on Windows, a linker
# script on Linux, or with -exported_symbols_list on OS X.
#
# The NSS build system processes them using a series of sed replacements,
# but the Mozilla build system is already running a Python script to generate
# the file so it's simpler to just do the replacement in Python.

import buildconfig


def main(output, input):
    # There's a check in old-configure.in under the system-nss handling
    # that should match this.
    if buildconfig.substs['OS_ARCH'] not in ('Linux', 'Darwin'):
        print "Error: unhandled OS_ARCH %s" % buildconfig.substs['OS_ARCH']
        return 1
    is_linux = buildconfig.substs['OS_ARCH'] == 'Linux'

    with open(input, 'rb') as f:
        for line in f:
            line = line.rstrip()
            # Remove all lines containing ';-'
            if ';-' in line:
                continue
            # On non-Linux, remove all lines containing ';+'
            if not is_linux and ';+' in line:
                continue
            # Remove the string ' DATA '.
            line = line.replace(' DATA ', '')
            # Remove the string ';+'
            line = line.replace(';+', '')
            # Remove the string ';;'
            line = line.replace(';;', '')
            # If a ';' is present, remove everything after it,
            # and on non-Linux, remove it as well.
            i = line.find(';')
            if i != -1:
                if is_linux:
                    line = line[:i+1]
                else:
                    line = line[:i]
            # On non-Linux, symbols get an underscore in front.
            if line and not is_linux:
                output.write('_')
            output.write(line)
            output.write('\n')
