#!/usr/local/bin/perl
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
# The Original Code is the Netscape security libraries.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1994-2000 Netscape Communications Corporation.  All
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

# Input: -d dir -j javahcmd foo1 foo2 . . .
#        Compares generated "_jni/foo1.h" file with "foo1.class", and
#        generated "_jni/foo2.h" file with "foo2.class", etc.
#        (NOTE:  unlike its closely related cousin, outofdate.pl,
#                the "-d dir" must always be specified)
#        Runs the javahcmd on all files that are different.
#
# Returns: list of headers which are OLDER than corresponding class
#          files (non-existant class files are considered to be real old :-)

my $javah = "";
my $classdir = "";

while(1) {
    if ($ARGV[0] eq '-d') {
        $classdir = $ARGV[1];
        $classdir .= "/";
        shift;
        shift;
    } elsif($ARGV[0] eq '-j') {
        $javah = $ARGV[1];
        shift;
        shift;
    } else {
        last;
    }
}

if( $javah  eq "") {
    die "Must specify -j <javah command>";
}
if( $classdir eq "") {
    die "Must specify -d <classdir>";
}

foreach $filename (@ARGV)
{
    $headerfilename = "_jni/";
    $headerfilename .= $filename;
    $headerfilename =~ s/\./_/g;
    $headerfilename .= ".h";

    $classfilename = $filename;
    $classfilename =~ s|\.|/|g;
    $classfilename .= ".class";

    $classfilename = $classdir . $classfilename;


    ( $dev, $ino, $mode, $nlink, $uid, $gid, $rdev, $size, $atime, $headermtime,
      $ctime, $blksize, $blocks ) = stat( $headerfilename );

    ( $dev, $ino, $mode, $nlink, $uid, $gid, $rdev, $size, $atime, $classmtime,
      $ctime, $blksize, $blocks ) = stat( $classfilename );

    if( $headermtime < $classmtime )
    {
	# NOTE:  Since this is used by "javah", and "javah" refuses to overwrite
	#        an existing file, we force an unlink from this script, since
	#        we actually want to regenerate the header file at this time.
        unlink $headerfilename;
        push @filelist, $filename;
    }
}

if( @filelist ) {
    $cmd = "$javah " . join(" ",@filelist);
    print "$cmd\n";
    system("$cmd");
} else {
    print "All JNI header files up to date.\n"
}
