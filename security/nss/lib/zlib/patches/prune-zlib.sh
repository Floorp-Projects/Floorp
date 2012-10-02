# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

files_to_remove="
CMakeLists.txt
ChangeLog
FAQ
INDEX
Makefile
Makefile.in
amiga
configure
contrib
doc
examples
make_vms.com
msdos
nintendods
old
qnx
treebuild.xml
watcom
win32
zconf.h.cmakein
zconf.h.in
zlib.3
zlib.3.pdf
zlib.map
zlib.pc.in
zlib2ansi
"

rm -rf $files_to_remove
