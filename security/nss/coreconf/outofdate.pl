#!/usr/local/bin/perl
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#Input: [-d dir] foo1.java foo2.java
#Compares with: foo1.class foo2.class (if -d specified, checks in 'dir', 
#  otherwise assumes .class files in same directory as .java files)
#Returns: list of input arguments which are newer than corresponding class
#files (non-existent class files are considered to be real old :-)

$found = 1;

if ($ARGV[0] eq '-d') {
    $classdir = $ARGV[1];
    $classdir .= "/";
    shift;
    shift;
} else {
    $classdir = "./";
}

foreach $filename (@ARGV) {
    $classfilename = $classdir;
    $classfilename .= $filename;
    $classfilename =~ s/.java$/.class/;
    ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,
     $ctime,$blksize,$blocks) = stat($filename);
    ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$classmtime,
     $ctime,$blksize,$blocks) = stat($classfilename);
#    print $filename, " ", $mtime, ", ", $classfilename, " ", $classmtime, "\n";
    if ($mtime > $classmtime) {
        print $filename, " ";
        $found = 0;
    }
}

print "\n";
