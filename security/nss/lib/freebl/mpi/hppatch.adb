#/bin/sh
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# script to change the system id in an object file from PA-RISC 2.0 to 1.1

adb -w $1 << EOF
?m 0 -1 0
0x0?X
0x0?W (@0x0&~0x40000)|(~@0x0&0x40000)

0?"change checksum"
0x7c?X
0x7c?W (@0x7c&~0x40000)|(~@0x7c&0x40000)
$q
EOF

exit 0

