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
# Contributor(s): Chris McAfee <mcafee@netscape.com>

#
# scrape.pl - Process log-scraped data into scrape.dat
#   Write data to $tree/scrape.dat in the following format,
#
#  <logfilename>|blurb1|blurb2|blurb3 ...
#

sub usage {
  warn "./scrape.pl <tree> <logfile>";
}

use FileHandle;

# This is for gunzip (Should add a configure script to handle this).
$ENV{PATH} .= ":/usr/local/bin";

unless ($#ARGV == 1) {
  &usage;
  die "Error: Wrong number of arguments\n";
}

($tree, $logfile) = @ARGV;

die "Error: No tree named $tree" unless -r "$tree/treedata.pl";
require "$tree/treedata.pl";

# Seach the build log for the scrape data
#
$fh = new FileHandle "gunzip -c $tree/$logfile |" 
  or die "Unable to open $tree/$logfile\n";
@scrape_data = find_scrape_data($fh);
$fh->close;

die "No scrape data found in log.\n" unless @scrape_data;

# Save the scrape data to 'scrape.dat'
#
open SCRAPE, ">>$tree/scrape.dat" or die "Unable to open $tree/scrape.dat";
print SCRAPE "$logfile|".join('|', @scrape_data)."\n";
close SCRAPE;


# end of main
#============================================================

sub find_scrape_data {
  my ($fh) = $_[0];
  local $_;
  my $inBloatStats = 0;

  while (<$fh>) {
    if ($inBloatStats and /^TOTAL/) {
      # Line format:
      #  TOTAL <absolute leaks> <% leaks delta> <absolute bloat> <% bloat delta>
      chomp;
      return (split)[1,3];
    }
    elsif (not $inBloatStats and /^\#* BLOAT STATISTICS/) {
      $inBloatStats = 1;
    }
  }
  return ();
}
