#!/usr/bin/perl -w
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
# The Original Code is header-count.pl.
# 
# The Initial Developer of the Original Code is L. David Baron.
# Portions created by L. David Baron are Copyright (C) 2000 L. David
# Baron.  All Rights Reserved.
# 
# Contributor(s):
#   L. David Baron <dbaron@fas.harvard.edu> (original author)

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
