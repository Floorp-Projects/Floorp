#!/bin/bash
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
# The Original Code is the Bugzilla Testopia System.
#
# The Initial Developer of the Original Code is Vance Baarda.
# Portions created by Vance Baarda are Copyright (C) 2006
# Novell. All Rights Reserved.
#
# Contributor(s): Vance Baarda <vrb@novell.com>

# This script can be used to update your version of Testopia to the 
# latest CVS tip.

# It checks out testopia's files into /tmp and copies them to the
# directory you specify, usually your bugzilla root directory.

(( $# == 1 )) ||
{
    echo "Usage: ${0##*/} docroot" >&2
    exit 1
}
docroot=$1

[[ $docroot =~ '^/.*' ]] ||
{
    echo docroot needs to be an absolute path >&2
    exit 1
}
[ -d $docroot ] ||
{
    echo $docroot: no such directory >&2
    exit 1
}

rm -rf /tmp/mozilla
cd /tmp
cvs -d :pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot co mozilla/webtools/testopia
cd mozilla/webtools/testopia
find . -name CVS -type d |
    while read name
    do
        rm -rf "$name"
    done
find * | cpio -pdumv $docroot
rm -rf /tmp/mozilla
