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
#
# One difference between the NSS build system and Mozilla's is that
# Mozilla's supports building on Linux for Windows using MinGW. MinGW
# expects all lines containing ;+ removed and all ;- tokens removed.

import buildconfig


def main(output, input):
    is_darwin = buildconfig.substs['OS_ARCH'] == 'Darwin'
    is_mingw = "WINNT" == buildconfig.substs['OS_ARCH'] and buildconfig.substs['GCC_USE_GNU_LD']

    with open(input, 'rb') as f:
        for line in f:
            line = line.rstrip()
            # On everything except MinGW, remove all lines containing ';-'
            if not is_mingw and ';-' in line:
                continue
            # On OS X, remove all lines containing ';+'
            if is_darwin and ';+' in line:
                continue
            # Remove the string ' DATA '.
            line = line.replace(' DATA ', '')
            # Remove the string ';+'
            if not is_mingw:
                line = line.replace(';+', '')
            # Remove the string ';;'
            line = line.replace(';;', '')
            # If a ';' is present, remove everything after it,
            # and on OS X, remove it as well.
            i = line.find(';')
            if i != -1:
                if is_darwin or is_mingw:
                    line = line[:i]
                else:
                    line = line[:i+1]
            # On OS X, symbols get an underscore in front.
            if line and is_darwin:
                output.write('_')
            output.write(line)
            output.write('\n')
