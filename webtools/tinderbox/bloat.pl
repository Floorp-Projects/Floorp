#! /usr/bonsaitools/bin/perl
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is Tinderbox
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1999
# Netscape Communications Corporation. All Rights Reserved.
#
# Contributor(s): Stephen Lamm <slamm@netscape.com>

#
# bloat.pl - Process bloat data from warren@netscape.com's bloat tool.
#   Write data to $tree/bloat.dat in the following format,
#
#  <logfilename>|<% leaks increase>|<% bloat increase>
#

sub usage {
  warn "./bloat.pl <tree> <logfile>";
}

use FileHandle;

# This is for gunzip (should add a configure script to handle this).
$ENV{PATH} .= ":/usr/local/bin";

unless ($#ARGV == 1) {
  &usage;
  die "Error: Wrong number of arguments\n";
}

($tree, $logfile) = @ARGV;

die "Error: No tree named $tree" unless -r "$tree/treedata.pl";
require "$tree/treedata.pl";

# Seach the build log for the bloat data
#
$fh = new FileHandle "gunzip -c $tree/$logfile |" 
  or die "Unable to open $tree/$logfile\n";
($leaks, $bloat) = find_bloat_data($fh);
$fh->close;

unless (defined $leaks and defined $bloat) {
  die "No bloat data found in log.\n";
}

# Save the bloat data to 'bloat.dat'
#
open BLOAT, ">>$tree/bloat.dat" or die "Unable to open $tree/bloat.dat";
print BLOAT "$logfile|$leaks|$bloat\n";
close BLOAT;


# end of main
#============================================================

sub find_bloat_data {
  my ($fh) = $_[0];
  local $_;
  my $inBloatStats = 0;

  while (<$fh>) {
    if ($inBloatStats) {
      if (/^TOTAL/) {
        chomp;
        # 2,4 is percentage, 1,3 is absolute.
        #my ($leaks, $bloat) = (split)[2,4];
        my ($leaks, $bloat) = (split)[1,3];
        chop $leaks;
        chop $bloat;
        return $leaks, $bloat;
      }
    } else {
      $inBloatStats = 1 if /^\#* BLOAT STATISTICS/;
    }
  }
  return undef, undef;
}
