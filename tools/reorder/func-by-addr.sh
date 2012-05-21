#!/bin/sh
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# Print the code symbols from an ELF file.
#

# Find the text segment by searching through the section headers
TEXT=`readelf --section-headers $1 | awk '$2 == ".text" { print $1; }' | sed -e 's/\[//g' -e 's/\]//g'`

# Now print all of the FUNC-type symbols in the text segment, including
# address, size, and symbol name.
readelf --syms $1 |
    awk "\$4 == \"FUNC\" && \$7 == \"${TEXT}\" { printf \"%s %8d %s\n\", \$2, \$3, \$8; }" |
    sort -u

