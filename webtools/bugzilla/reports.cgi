#!/usr/bonsaitools/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.
#
# Contributor(s): Harrison Page <harrison@netscape.com>,
# Terry Weissman <terry@mozilla.org>

use diagnostics;
use strict;

require "CGI.pl";
require "globals.pl";

use vars @::legal_product;

my $week = 60 * 60 * 24 * 7;
my @status = qw (NEW ASSIGNED REOPENED);

ConnectToDatabase();

print <<FIN;
Content-type: text/html

<html>
<head>
<title>Bugzilla Reports</title>
</head>
<body bgcolor="#FFFFFF">
FIN

&header unless (defined $::FORM{'nobanner'});

if (! defined $::FORM{'product'})
	{
	&choose_product;
	}
else
	{
	&most_doomed;
	}

print <<FIN;
<p>
</body>
</html>
FIN

##################################
# user came in with no form data #
##################################

sub choose_product
	{
	GetVersionTable();
	
	my $product_popup = make_options (\@::legal_product, $::legal_product[0]);

	print <<FIN;
<center>
<h1>Welcome to the Bugzilla Query Kitchen</h1>
<form method=get action=reports.cgi>
<table border=1 cellpadding=5>
<tr>
<td align=center><b>Product:</b></td>
<td align=center>
<select name="product">
$product_popup
</select>
</td>
</tr>
<tr>
<td align=center><b>Switches:</b></td>
<td align=left>
<input type=checkbox name=links value=1>&nbsp;Links to Bugs<br>
<input type=checkbox name=nobanner value=1>&nbsp;No Banner<br>
</td>
</tr>
<tr>
<td colspan=2 align=center>
<input type=submit value=Continue>
</td>
</tr>
</table>
</form>
<p>
FIN
	}

##########################
# they sent us a project #
##########################

sub most_doomed
	{
	my $when = localtime (time);

	print <<FIN;
<center>
<h1>
Bug Report for $::FORM{'product'}
</h1>
$when<p>
FIN

	my $query = <<FIN;
select 
	bugs.bug_id, bugs.assigned_to, bugs.bug_severity,
	bugs.bug_status, bugs.product, 
	assign.login_name,
	report.login_name,
	unix_timestamp(date_format(bugs.creation_ts, '%Y-%m-%d %h:%m:%s'))

from   bugs,
       profiles assign,
       profiles report,
       versions projector
where  bugs.assigned_to = assign.userid
and    bugs.reporter = report.userid
and    bugs.product='$::FORM{'product'}'
and 	 
	( 
	bugs.bug_status = 'NEW' or 
	bugs.bug_status = 'ASSIGNED' or 
	bugs.bug_status = 'REOPENED'
	)
FIN

	print "<font color=purple><tt>$query</tt></font><p>\n" 
		unless (! exists $::FORM{'showsql'});

	SendSQL ($query);
	
	my $c = 0;

	my $bugs_count = 0;
	my $bugs_new_this_week = 0;
	my $bugs_reopened = 0;
	my %bugs_owners;
	my %bugs_summary;
	my %bugs_status;
	my %bugs_totals;
	my %bugs_lookup;

	#############################
	# suck contents of database # 
	#############################

	while (my ($bid, $a, $sev, $st, $prod, $who, $rep, $ts) = FetchSQLData())
		{
		next if (exists $bugs_lookup{$bid});
		
		$bugs_lookup{$bid} ++;
		$bugs_owners{$who} ++;
		$bugs_new_this_week ++ if (time - $ts <= $week);
		$bugs_status{$st} ++;
		$bugs_count ++;
		
		push @{$bugs_summary{$who}{$st}}, $bid;
		
		$bugs_totals{$who}{$st} ++;
		}

	#########################
	# start painting report #
	#########################

	print <<FIN;
<h1>Summary</h1>
<table border=1 cellpadding=5>
<tr>
<td align=right><b>New Bugs This Week</b></td>
<td align=center>$bugs_new_this_week</td>
</tr>

<tr>
<td align=right><b>Bugs Marked New</b></td>
<td align=center>$bugs_status{'NEW'}</td>
</tr>

<tr>
<td align=right><b>Bugs Marked Assigned</b></td>
<td align=center>$bugs_status{'ASSIGNED'}</td>
</tr>

<tr>
<td align=right><b>Bugs Marked Reopened</b></td>
<td align=center>$bugs_status{'REOPENED'}</td>
</tr>

<tr>
<td align=right><b>Total Bugs</b></td>
<td align=center>$bugs_count</td>
</tr>

</table>
<p>
FIN

	if ($bugs_count == 0)
		{
		print "No bugs found!\n";
		exit;
		}
	
	print <<FIN;
<h1>Bug Count by Engineer</h1>
<table border=3 cellpadding=5>
<tr>
<td align=center bgcolor="#DDDDDD"><b>Owner</b></td>
<td align=center bgcolor="#DDDDDD"><b>New</b></td>
<td align=center bgcolor="#DDDDDD"><b>Assigned</b></td>
<td align=center bgcolor="#DDDDDD"><b>Reopened</b></td>
<td align=center bgcolor="#DDDDDD"><b>Total</b></td>
</tr>
FIN

	foreach my $who (sort keys %bugs_summary)
		{
		my $bugz = 0;
	 	print <<FIN;
<tr>
<td align=left><tt>$who</tt></td>
FIN
		
		foreach my $st (@status)
			{
			$bugs_totals{$who}{$st} += 0;
			print <<FIN;
<td align=center>$bugs_totals{$who}{$st}
FIN
			$bugz += $#{$bugs_summary{$who}{$st}} + 1;
			}
		
		print <<FIN;
<td align=center>$bugz</td>
</tr>
FIN
		}
	
	print <<FIN;
</table>
<p>
FIN

	###############################
	# individual bugs by engineer #
	###############################

	print <<FIN;
<h1>Individual Bugs by Engineer</h1>
<table border=1 cellpadding=5>
<tr>
<td align=center bgcolor="#DDDDDD"><b>Owner</b></td>
<td align=center bgcolor="#DDDDDD"><b>New</b></td>
<td align=center bgcolor="#DDDDDD"><b>Assigned</b></td>
<td align=center bgcolor="#DDDDDD"><b>Reopened</b></td>
</tr>
FIN

	foreach my $who (sort keys %bugs_summary)
		{
		print <<FIN;
<tr>
<td align=left><tt>$who</tt></td>
FIN

		foreach my $st (@status)
			{
			my @l;

			foreach (sort { $a <=> $b } @{$bugs_summary{$who}{$st}})
				{
				if ($::FORM{'links'})
					{
					push @l, "<a href=\"show_bug.cgi?id=$_\">$_</a>\n"; 
					}
				else
					{
					push @l, $_;
					}
				}
				
			my $bugz = join ' ', @l;
			$bugz = "&nbsp;" unless ($bugz);
			
			print <<FIN
<td align=left>$bugz</td>
FIN
			}

		print <<FIN;
</tr>
FIN
		}

	print <<FIN;
</table>
<p>
FIN
	}

sub header
	{
	print <<FIN;
<TABLE BGCOLOR="#000000" WIDTH="100%" BORDER=0 CELLPADDING=0 CELLSPACING=0>
<TR><TD><A HREF="http://www.mozilla.org/"><IMG
 SRC="http://www.mozilla.org/images/mozilla-banner.gif" ALT=""
 BORDER=0 WIDTH=600 HEIGHT=58></A></TD></TR></TABLE>
FIN
	}
