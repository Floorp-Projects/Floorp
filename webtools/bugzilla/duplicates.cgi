#!/usr/bonsaitools/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
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
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Gervase Markham <gerv@gerv.net>
#
# Generates mostfreq list from data collected by collectstats.pl.


use diagnostics;
use strict;
use CGI "param";
use AnyDBM_File;
require "globals.pl";
require "CGI.pl";

ConnectToDatabase(1);
GetVersionTable();

quietly_check_login();

# Silly used-once warnings
$::userid = $::userid;
$::usergroupset = $::usergroupset;

my %dbmcount;
my %count;
my $dobefore = 0;
my $before = "";
my %before;

# Get params from URL

my $changedsince = 7;     # default one week
my $maxrows = 100;        # arbitrary limit on max number of rows
my $sortby = "dup_count"; # default to sorting by dup count

if (defined(param("sortby")))
{
  $sortby = param("sortby");
}

# Check for changedsince param, and see if it's a positive integer
if (defined(param("changedsince")) && param("changedsince") =~ /^\d{1,4}$/) 
{
  $changedsince = param("changedsince");
}

# check for max rows param, and see if it's a positive integer
if (defined(param("maxrows")) && param("maxrows") =~ /^\d{1,4}$/)
{
  $maxrows = param("maxrows");
}

# Start the page
print "Content-type: text/html\n";
print "\n";
PutHeader("Most Frequently Reported Bugs");

# Open today's record of dupes
my $today = &days_ago(0);

if (<data/duplicates/dupes$today*>)
{
  dbmopen(%dbmcount, "data/duplicates/dupes$today", 0644) || 
                            &die_politely("Can't open today's dupes file: $!");
}
else
{
  # Try yesterday's, then (in case today's hasn't been created yet)
  $today = &days_ago(1);
  if (<data/duplicates/dupes$today*>)
  {
    dbmopen(%dbmcount, "data/duplicates/dupes$today", 0644) || 
                        &die_politely("Can't open yesterday's dupes file: $!");
  }
  else
  {
    &die_politely("There are no duplicate statistics for today ($today) or yesterday.");
  }
}

# Copy hash (so we don't mess up the on-disk file when we remove entries)
%count = %dbmcount;
my $key;
my $value;
my $threshold = Param("mostfreqthreshold");

# Remove all those dupes under the threshold (for performance reasons)
while (($key, $value) = each %count)
{
  if ($value < $threshold)
  {
    delete $count{$key};
  } 
}

# Try and open the database from "changedsince" days ago
$before = &days_ago($changedsince);    

if (<data/duplicates/dupes$before*>) 
{
  dbmopen(%before, "data/duplicates/dupes$before", 0644) && ($dobefore = 1);
}

print Param("mostfreqhtml");

my %delta;

if ($dobefore) 
{
  # Calculate the deltas if we are doing a "before"
  foreach (keys(%count))
  {
    $delta{$_} = $count{$_} - $before{$_};
  }
}

# Sort, if required
my @sortedcount;

if    ($sortby eq "delta")
{
  @sortedcount = sort by_delta keys(%count);
}
elsif ($sortby eq "bug_no")
{
  @sortedcount = reverse sort by_bug_no keys(%count);
}
elsif ($sortby eq "dup_count")
{
  @sortedcount = sort by_dup_count keys(%count);
}

my $i = 0;

# Produce a string of bug numbers for a Bugzilla buglist.
my $commabugs = "";
foreach (@sortedcount) 
{
  last if ($i == $maxrows);

  $commabugs .= ($_ . ",");
  $i++;  
}

# Avoid having a comma at the end - Bad Things happen.
chop $commabugs; 

print qq|

<form method="POST" action="buglist.cgi">
<input type="hidden" name="bug_id" value="$commabugs">
<input type="hidden" name="order" value="Reuse same sort as last time">
Give this to me as a <input type="submit" value="Bug List">. (Note: the order may not be the same.)
</form>

<table BORDER>
<tr BGCOLOR="#CCCCCC">

<td><center><b>
<a href="duplicates.cgi?sortby=bug_no&maxrows=$maxrows&changedsince=$changedsince">Bug #</a>
</b></center></td>
<td><center><b>
<a href="duplicates.cgi?sortby=dup_count&maxrows=$maxrows&changedsince=$changedsince">Dupe<br>Count</a>
</b></center></td>\n|;

if ($dobefore) 
{
  print "<td><center><b>
  <a href=\"duplicates.cgi?sortby=delta&maxrows=$maxrows&changedsince=$changedsince\">Change in
  last<br>$changedsince day(s)</a></b></center></td>";
}

print "
<td><center><b>Component</b></center></td>
<td><center><b>Severity</b></center></td>
<td><center><b>Op Sys</b></center></td>
<td><center><b>Target<br>Milestone</b></center></td>
<td><center><b>Summary</b></center></td>
</tr>\n\n";

$i = 0;

foreach (@sortedcount)
{
  my $id = $_;
  SendSQL(SelectVisible("SELECT component, bug_severity, op_sys, target_milestone, short_desc, bug_status, resolution" .
                 " FROM bugs WHERE bugs.bug_id = $id", $::userid, $::usergroupset));
  next unless MoreSQLData();
  my ($component, $severity, $op_sys, $milestone, $summary, $bug_status, $resolution) = FetchSQLData();
  $summary = html_quote($summary);

  # Show all bugs except those CLOSED _OR_ VERIFIED but not INVALID or WONTFIX.
  # We want to see VERIFIED INVALID and WONTFIX because common "bugs" which aren't
  # bugs end up in this state.
  unless ( ($bug_status eq "CLOSED") || ( ($bug_status eq "VERIFIED") &&
          ! ( ($resolution eq "INVALID") || ($resolution eq "WONTFIX") ) ) ) {
    print "<tr>";
    print '<td><center>';
    if  ( ($bug_status eq "RESOLVED") || ($bug_status eq "VERIFIED") ) {
      print "<strike>";
    }
    print "<A HREF=\"show_bug.cgi?id=" . $id . "\">";
    print $id . "</A>";
    if  ( ($bug_status eq "RESOLVED") || ($bug_status eq "VERIFIED") ) {
      print "</strike>";
    }
    print "</center></td>";
    print "<td><center>$count{$id}</center></td>";
    if ($dobefore) 
    {
      print "<td><center>$delta{$id}</center></td>";
    }
    print "<td>$component</td>\n    ";
    print "<td><center>$severity</center></td>";
    print "<td><center>$op_sys</center></td>";
    print "<td><center>$milestone</center></td>";
    print "<td>$summary</td>";
    print "</tr>\n";

    $i++;
  }

  if ($i == $maxrows)
  {
    last;
  }
}

print "</table><br><br>";
PutFooter();


sub by_bug_no 
{
  return ($a <=> $b);
}

sub by_dup_count 
{
  return -($count{$a} <=> $count{$b});
}

sub by_delta 
{
  return -($delta{$a} <=> $delta{$b});
}

sub days_ago 
{
  my ($dom, $mon, $year) = (localtime(time - ($_[0]*24*60*60)))[3, 4, 5];
  return sprintf "%04d-%02d-%02d", 1900 + $year, ++$mon, $dom;
}

sub die_politely {
  my $msg = shift;

  print <<FIN;
<p>
<table border=1 cellpadding=10>
<tr>
<td align=center>
<font color=blue>$msg</font>
</td>
</tr>
</table>
<p>
FIN
    
  PutFooter();
  exit;
}
