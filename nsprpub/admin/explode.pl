#!/bin/perl
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
# The Original Code is the Netscape Portable Runtime (NSPR).
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1998-2001 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
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
# 
# -----------------------------------------------------------------
#
# explode.pl -- Unpack .jar files into bin, lib, include directories
#
# syntax: perl explode.pl
#
# Description:
# explode.pl unpacks the .jar files created by the NSPR build   
# procedure. 
#
# Suggested use: After copying the platform directories to
# /s/b/c/nspr20/<release>. CD to /s/b/c/nspr20/<release> and
# run explode.pl. This will unpack the jar files into bin, lib,
# include directories.
#
# -----------------------------------------------------------------

@dirs = `ls -d *.OBJ*`;

foreach $dir (@dirs) {
    chop($dir);
    if (-l $dir) {
        print "Skipping symbolic link $dir\n";
        next;
    }
    print "Unzipping $dir/mdbinary.jar\n";
    system ("unzip", "-o", "$dir/mdbinary.jar",
            "-d", "$dir");
    system ("rm", "-rf", "$dir/META-INF");
    mkdir "$dir/include", 0755;
    print "Unzipping $dir/mdheader.jar\n";
    system ("unzip", "-o", "-aa",
            "$dir/mdheader.jar",
            "-d", "$dir/include");
    system ("rm", "-rf", "$dir/include/META-INF");
}
# --- end explode.pl ----------------------------------------------
