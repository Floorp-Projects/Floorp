#!/usr/bin/perl
# -*- Mode: perl; indent-tabs-mode: nil -*-
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
# The Original Code is the Mozilla graph tool.
#
# The Initial Developer of the Original Code is AOL Time Warner, Inc.
#
# Portions created by AOL Time Warner, Inc. are Copyright (C) 2001
# AOL Time Warner, Inc.  All Rights Reserved.
#
# Contributor(s):
#   Chris McAfee <mcafee@mocha.com>
#   John Morrison <jrgm@liveops.com>
#

use CGI::Carp qw(fatalsToBrowser);
use CGI::Request;
use Date::Calc qw(Add_Delta_Days);  # http://www.engelschall.com/u/sb/download/Date-Calc/

my $req = new CGI::Request;

my $TESTNAME  = lc($req->param('testname'));
my $TBOX      = lc($req->param('tbox'));
my $DAYS      = lc($req->param('days'));
my $DATAFILE  = "db/$TESTNAME/$TBOX";


sub show_data {
  die "$TBOX: no data file found." 
	unless -e $DATAFILE; 

  #
  # At some point we might want to clip the data printed
  # based on days=n.

  # Set scaling for x-axis (time)
  #my $today;
  #my $n_days_ago;

  # Get current time, $today.
  #my ($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdat) = localtime();  
  #$year += 1900;
  #$mon += 1;
  #$today = sprintf "%04d:%02d:%02d:%02d:%02d:%02d", $year, $mon, $mday, $hour, $min, $sec;

  # Calculate date $DAYS before $today.
  #my ($year2, $mon2, $mday2) = Add_Delta_Days($year, $mon, $mday, -$DAYS);
  #$n_days_ago= sprintf "%04d:%02d:%02d:%02d:%02d:%02d", $year2, $mon2, $mday2, $hour, $min, $sec;

  print "<h3>Data for $TBOX $TESTNAME</h3>\n";

  # links
  print "<li><a href=\"#plot\">plot points</a></li>\n";
  print "<li><a href=\"#raw\">raw points</a></li>\n";
  print "<br>\n";

  # Print data that got plotted.
  print "<a name=\"plot\"><b>Data in plot form: #, (t, y)</b><br>\n";
  open DATA, "$DATAFILE" or die "Couldn't open file, $DATAFILE: $!";
  my $i = 0;
  print "<pre>\n";
  while (<DATA>) {
    my @line = split('\t',$_);
    $line_string = sprintf "%4d %s %s\n", $i, @line[0], @line[1];
    print "$line_string";
    $i++;
  }
  print "</pre>\n";
  close DATA;


  # Print data again, in raw format.
  print "<br>\n";
  print "<br>\n";
  print "<a name=\"raw\"><b>Data in raw form: #, (t, y, opaque data, ip, user agent)</b><br>\n";
  open DATA, "$DATAFILE" or die "Couldn't open file, $DATAFILE: $!";
  $i = 0;
  print "<pre>\n";
  while (<DATA>) {
    chomp($_);
    my $line_string = sprintf "%4d %s\n", $i, $_;
    print "$line_string";
    $i++;
  }
  print "</pre>\n";
  close DATA;  

}

# main
{
	
  print "Content-type: text/html\n\n<HTML>\n";
  print "<title>rawdata for $TBOX $TESTNAME</title><br>\n";
  print "<body>\n";

  if(!$TESTNAME) {
	print "Error: need testname.";
  } elsif(!$TBOX) {
	print "Error: need machine name.";
  } else {
	show_data();
  }
  print "</body>\n";
  print "</html>\n";
}

exit 0;

