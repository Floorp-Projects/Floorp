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
use POSIX qw(strftime);
use strict;

my $req  = new CGI::Request;

# incoming query string has the form: "?value=n&data=p:q:r...&tbox=foopy" 
# where 'n' is the average recorded time, and p,q,r... are the raw numbers,
# and 'tbox' is a name of a tinderbox

use vars qw{$value $data $tbox $testname $ua $ip $time};
$value    = $req->param('value');
$data     = $req->param('data');  # Opaque data, anything can go here.
$tbox     = $req->param('tbox'); $tbox =~ tr/A-Z/a-z/;
$testname = $req->param('testname');
$ua       = $req->cgi->var("HTTP_USER_AGENT");
$ip       = $req->cgi->var("REMOTE_ADDR");
$time     = strftime "%Y:%m:%d:%H:%M:%S", localtime;

# Testing, please leave this here.
# $value    = "1234";    #$req->param('value');
# $data     = "1:2:3:4"; #$req->param('data');
# $tbox     = "lespaul"; #$tbox =~ tr/A-Z/a-z/;
# $testname = "startup"; #$req->param('testname');
# $ua       = "ua";      #$req->cgi->var("HTTP_USER_AGENT");
# $ip       = "1.2.3.4"; #$req->cgi->var("REMOTE_ADDR");
# $time     = "now";     #strftime "%Y:%m:%d:%H:%M:%S", localtime;


print "Content-type: text/plain\n\n";
for (qw{value data tbox testname ua ip time}) {
    no strict 'refs';
    printf "%10s = %s\n", $_, $$_;
}

# close HTTP transaction, and then decide whether to record data
#
# XXXX I had to comment this out, not sure why.  startup version
#      of this cgi had this and worked.  -mcafee
#close(STDOUT); 

#my %nametable = read_config();

# punt fake inputs
#die "Yer a liar"
#     unless $ip eq $nametable{$tbox} or $ip eq '208.12.39.125';
#die "No 'real' browsers allowed: $ua" 
#     unless $ua =~ /^(libwww-perl|PERL_CGI_BASE)/;
die "No 'value' parameter supplied"
     unless (defined $value);  # Allow for value=0
die "No 'testname' parameter supplied"
     unless $testname;
die "No 'tbox' parameter supplied"
     unless $tbox;
die "No 'data' parameter supplied"
     unless $data;


# Test for dirs.
unless (-d "db") {
  mkdir("db", 0755);
}


unless (-d "db/$testname") {
  mkdir("db/$testname", 0755);
}

# If file doesn't exist, try creating empty file.
my $datafile = "db/$testname/$tbox";
unless (-f $datafile) {
  open(FILE, "> $datafile") || die "Can't create new file $datafile: $!";
  close(FILE);
}

# Record data.
open(FILE, ">> $datafile") ||
     die "Can't open $datafile: $!";
print FILE "$time\t$value\t$data\t$ip\t$tbox\t$ua\n";
close(FILE);

# Compute and record moving average.
# Use last 10 points, including the data we are recieving here.
my $num_pts = 10;

# Run through the data file, count data points.
my $total_pts = 0;
open(FILE, "db/$testname/$tbox");
  while (<FILE>) {
	$total_pts++;
  }
close(FILE);

# Don't compute average unless we have enough points.
if($total_pts >= $num_pts) {
  # Add up last 10 data points, get the average.
  my $i = 0;
  my @line_array;
  my $sum = 0;
  open(FILE, "db/$testname/$tbox");
  while (<FILE>) {
	if($i >= ($total_pts - $num_pts)) {
	  @line_array = split("\t","$_");
      $sum += $line_array[1];
	}
	$i++;
  }

  my $avg = $sum/$num_pts;
  close(FILE);

  # If average datafile doesn't exist, try creating empty file.
  my $datafile_avg = $datafile . "_avg";
  unless (-f $datafile_avg) {
    open(FILE, "> $datafile_avg") || die "Can't create new file $datafile_avg: $!";
    close(FILE);
  }

  # Write the data.
  open(FILE, ">> $datafile_avg") ||
       die "Can't open $datafile_avg: $!";
  print FILE "$time\t$avg\n";
  close(FILE);
}

exit 0;

#sub read_config() {
#    my %nametable;
#    open(CONFIG, "< db/config.txt") || 
#	 die "can't open config.txt: $!";
#    while (<CONFIG>) {
#	next if /^$/;
#	next if /^#|^\s+#/;
#	s/\s+#.*$//;
#	my ($tinderbox, $assigned_ip) = split(/\s+/, $_);
#	$nametable{$tinderbox} = $assigned_ip;
#    }
#    return %nametable;
#}
