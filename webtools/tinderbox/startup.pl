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
# startup.pl - Process startup data from mozilla/build/startup-test.html.
#   Write data to $tree/startup.dat in the following format,
#
#  <logfilename>|<startup time>
#

sub usage {
  warn "./startup.pl <tree> <logfile>";
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

# Seach the build log for the startup data
#
$fh = new FileHandle "gunzip -c $tree/$logfile |" 
  or die "Unable to open $tree/$logfile\n";
my $startup_time = 0;
$startup_time = find_startup_data($fh);
$fh->close;

die "No startup data found in log.\n" unless $startup_time;

print "startup_time = $startup_time\n";

# Save the startup data to 'startup.dat'
#
open STARTUP, ">>$tree/startup.dat" or die "Unable to open $tree/startup.dat";
print STARTUP "$logfile|$startup_time\n";
close STARTUP;


# end of main
#============================================================

sub find_startup_data {
  my ($fh) = $_[0];
  local $_;

  # Search for "__avg_startuptime" string, and pick off 2nd item.
  while (<$fh>) {
    if (/^  __avg_startuptime/) {
      # Line format, time in ms:
      #  __avg_startuptime,<time>
      # 
      chomp;
      print "\$_ = $_\n";
      @findLine = split(/,/,$_);

      # Pick off 2nd item, startup time.
      return $findLine[1];
    } 
  }
  return ();
}
