#/bin/sh
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is script to change the system id in an object file 
# from PA-RISC 2.0 to 1.1.
#
# The Initial Developer of the Original Code is Hewlett-Packard Company.
# Portions created by Hewlett-Packard Company are
# Copyright (C) March 1999, Hewlett-Packard Company. All Rights Reserved.
#
# Contributor(s):
#   wrapped by Dennis Handly on Tue Mar 23 15:23:43 1999
#
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable
# instead of those above.  If you wish to allow use of your
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.

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

