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
use DB_File;
require "globals.pl";
require "CGI.pl";

ConnectToDatabase();
GetVersionTable();

my %count;
my $dobefore = 0;
my $before = "";
my %before;

my $changedsince;
my $maxrows = 500;       # arbitrary limit on max number of rows

my $today = &days_ago(0);

# Open today's record of dupes
if (-e "data/mining/dupes$today.db")
{
	dbmopen(%count, "data/mining/dupes${today}.db", 0644) || die "Can't open today's dupes file: $!";
}
else
{
	# Try yesterday's, then (in case today's hasn't been created yet) :-)
	$today = &days_ago(1);
	if (-e "data/mining/dupes$today.db")
	{
		dbmopen(%count, "data/mining/dupes${today}.db", 0644) || die "Can't open yesterday's dupes file: $!";
	}
	else
	{
		die "There are no duplicate statistics for today or yesterday.";
	}
}

# Check for changedsince param, and see if it's a positive integer
if (defined(param("changedsince")) && param("changedsince") =~ /^\d{1-4}$/) 
{
	$changedsince = param("changedsince");
}
else
{
	# Otherwise, default to one week
	$changedsince = "7";
}

$before = &days_ago($changedsince);		

# check for max rows parameter
if (defined(param("maxrows")) && param("maxrows") =~ /^\d{1-4}$/)
{
	$maxrows = param("maxrows");
}

if (-e "data/mining/dupes${before}.db") 
{
	dbmopen(%before, "data/mining/dupes${before}.db", 0644) && ($dobefore = 1);
}

print "Content-type: text/html\n";
print "\n";
PutHeader("Most Frequently Reported Bugs");

print Param("mostfreqhtml");

print "
<table BORDER>

<tr BGCOLOR=\"#CCCCCC\">
<td><center><b>Bug #</b></center></td>
<td><center><b>Dupe<br>Count</b></center></td>\n";

if ($dobefore) 
{
	print "<td><center><b>Change in last<br>$changedsince day(s)</b></center></td> ";
}

print "
<td><center><b>Component</b></center></td>
<td><center><b>Severity</b></center></td>
<td><center><b>Op Sys</b></center></td>
<td><center><b>Target<br>Milestone</b></center></td>
<td><center><b>Summary</b></center></td>
</tr>\n\n";

my %delta;

# Calculate the deltas if we are doing a "before"
if ($dobefore)
{
	foreach (keys(%count))
	{
		$delta{$_} = $count{$_} - $before{$_};
	}
}

# Offer the option of sorting on total count, or on the delta
my @sortedcount;

if (defined(param("sortby")) && param("sortby") == "delta")
{
	@sortedcount = sort by_delta keys(%count);
}
else
{
	@sortedcount = sort by_dup_count keys(%count);
}

my $i = 0;

foreach (@sortedcount)
{
	my $id = $_;
	SendSQL("SELECT component, bug_severity, op_sys, target_milestone, short_desc FROM " . 
												   "bugs WHERE bug_id = $id");
	my ($component, $severity, $op_sys, $milestone, $summary) = FetchSQLData();
	print "<tr>";
	print '<td><center><A HREF="show_bug.cgi?id=' . $id . '">';
	print $id . "</A></center></td>";
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
	if ($i == $maxrows)
	{
		last;
	}
}

print "</table><br><br>";
PutFooter();


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
    return sprintf "%04d%02d%02d", 1900 + $year, ++$mon, $dom;
}

