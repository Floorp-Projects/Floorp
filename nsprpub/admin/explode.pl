#!/bin/perl
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
