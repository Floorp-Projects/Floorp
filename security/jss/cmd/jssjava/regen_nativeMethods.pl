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
# The Original Code is the Netscape Security Services for Java.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1998-2000 Netscape Communications Corporation.  All
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

# Input: -d dir generated_file foo1 foo2 . . .
#        Compares generated file with "_jni/foo1.h", and
#        generated file with "_jni/foo2.h", etc.
#
#        (NOTE:  unlike its closely related cousin, outofdate.pl,
#                the "-d dir" must always be specified; also, unlike
#                its closely related cousin, jniregen.pl, if the generated file
#                is older than ANY "_jni/foo?.h", then the generated file will
#                be regenerated in its entirety, rather than just the portions
#                associated with the list of files returned by this script)
#
# Returns: list of headers which are NEWER than corresponding class
#          files (non-existant header files are considered to be real old :-)

$found = 1;

if ($ARGV[0] eq '-d')
{
    $headerdir = $ARGV[1];
    $headerdir .= "/";
    shift;
    shift;
}
else
{
    print STDERR "Usage:  perl ", $0, " -d dir generated_file foo1 foo2 . . .\n";
    exit -1;
}

$generatedfilename = $ARGV[0];
shift;

foreach $filename (@ARGV)
{
    $headerfilename = $headerdir;
    $headerfilename .= $filename;
    $headerfilename =~ s/\./_/g;
    $headerfilename .= ".h";

    ( $dev, $ino, $mode, $nlink, $uid, $gid, $rdev, $size, $atime, $generatedmtime,
      $ctime, $blksize, $blocks ) = stat( $generatedfilename );

    ( $dev, $ino, $mode, $nlink, $uid, $gid, $rdev, $size, $atime, $headermtime,
      $ctime, $blksize, $blocks ) = stat( $headerfilename );

    if( $headermtime > $generatedmtime )
    {
        print $filename, " ";
        $found = 0;
    }
}

print "\n";
exit 0;

