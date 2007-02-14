#!/usr/bin/perl -w
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is header-count.pl.
#
# The Initial Developer of the Original Code is
# L. David Baron.
# Portions created by the Initial Developer are Copyright (C) 2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   L. David Baron <dbaron@dbaron.org> (original author)
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

# This script uses the gcc-generated dependency files in .deps
# directories to count the number of C/C++ files for which each header
# file is brought in.

use strict;

unless ($#ARGV == 0) {
    print "Usage: $0 <object-directory>\n";
    exit;
}

my $dirname = $ARGV[0];
my %hfilecounts;
my $cfilecount;

open(FILELIST, "find $dirname -path '*/.deps/*.pp' ! -name '.all.pp' |");

while (my $filename = <FILELIST>) {
    open(DEPSFILE, "$filename");
    $cfilecount++;
    while (<DEPSFILE>) {
        my @words = split();
        foreach my $word (@words) {
            if ($word =~ /^(\/usr.*\.h)$/) {
                # /usr*.h - use entire path
                $hfilecounts{$1}++;
            } elsif ($word =~ /\/([^\/]*\.h)$/) {
                # other .h files - use just the filename
                $hfilecounts{$1}++;
            }
        }
    }
}

print "$cfilecount files total.\n\n";
my @sortednames =
  sort {$hfilecounts{$b} <=> $hfilecounts{$a}} keys(%hfilecounts);
foreach my $filename (@sortednames) {
    print "$hfilecounts{$filename} $filename\n";
}
