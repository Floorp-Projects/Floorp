#!/usr/bonsaitools/bin/perl
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
# The Original Code is the Mozilla Miscellaneous Statistics Generator.
# 
# The Initial Developer of the Original Code is mozilla.org.
# Portions created by mozilla.org are
# Copyright (C) 1999.  All
# Rights Reserved.
# 
# Contributor(s): Dan Mosedale <dmose@mozilla.org>
#                 Myk Melez <myk@mozilla.org>
#

#
# $Id: genstats.cgi,v 1.15 2005/12/19 00:47:50 timeless%mozdev.org Exp $ 
#
# generate statistics related to non-Netscape participation in mozilla.org
#

use diagnostics;
use strict;

use Mysql;
use CGI::Carp qw(fatalsToBrowser);
use CGI qw(:standard :html3);
use Date::Format;

@::STATINFO = (

	["Active checkers-in (source only)",
						# statistic title
	 "bonsai",				# database
	 "checkins.ci_when",			# timestamp field
	 "people.who",				# email addr field
	 "select distinct people.id from checkins,people where " . 
		"people.id=checkins.whoid"
	],

	["New checkers-in (source)",
						# statistic title
	 "mozusers",				# database
	 "changes.changed_when",		# timestamp field
	 "changes.email",			# email addr field
	 "select distinct id from changes,users where changes.field=" .
		"'cvs_group' and changes.email=users.email ".
	        "and changes.oldvalue='None'"
	],

	["New checkers-in (docs)",
						# statistic title
	 "mozusers",				# database
	 "changes.changed_when",		# timestamp field
	 "changes.email",			# email addr field
	 "select distinct id from changes,users where changes.field=" .
		"'gila_group' and changes.email=users.email ".
	        "and changes.oldvalue='None'"
	],

	["Useful patches (as attributed in CVS logs)",
					 	# statistic title
	 "bonsai",				# database
	 "checkins.ci_when",			# timestamp field
	 "descs.description",			# email addr field
	 'select distinct description from checkins,descs where ' .
		'checkins.descid=descs.id and description regexp ' . 
		'"[[:<:]][a-zA-Z0-9_\\.\\-]+@[a-zA-Z0-9_\\.\\-]+[[:>:]]"'
	],

	["Checkins",			# title
	 "bonsai",				# database 
         "ci_when",				# timestamp field
	 "people.who",				# email addr field
	 "select distinct descs.description from people,checkins,descs " .
		"where checkins.descid=descs.id and people.id=checkins.whoid"
	],

	["Files checked in",		# title
	 "bonsai",				# database 
         "ci_when",				# timestamp field
	 "people.who",				# email addr field
	 "select fileid from people,checkins where people.id=checkins.whoid"
	]		 
);

@::STATS = ();

if (!param()) {

	print header();
	print start_html("-title"=>'mozilla.org statistics generation form',
			 -BGCOLOR=>'#FFFFFF',
			 -TEXT=>'#000000');

	print start_form("Get");
	print "generate stats for the last ";
	print textfield(-name=>"months",
			-default=>8,
			-size=>5,
			-override=>1);
	print "months";
	print p();

	print "mozilla.org is considered part of Netscape? ";
	print radio_group(-name=>"mozillaOrgAsNscp",
			  "-values"=>["yes","no"],
			  -default=>"no");
	print p();

	print "include the current (incomplete) month?";
	print radio_group(-name=>"includeCurrentMonthP",
			  "-values"=>["yes", "no"],
			  -default=>"yes");
	print p();

	print submit("generate statistics");

	print p();
	print "please be patient: generating statistics is likely to take " .
	      "between one and ten minutes.";
	print end_form();

	print end_html();
	exit();
}

# for unclear reasons, this is necessary to prevent a warning
#
$F::includeCurrentMonthP = "stuff";
$F::debugHtml = 0;

# put all vars from the form in $F::
#
import_names("F", 1);		

$::db = Mysql->Connect("localhost")
	|| die "Can't connect to database server";

# figure out the last month that we want stats for (whatever month it is 
# right now)
#
my $lastMonth = time2str("%m", time());
my $lastYear = time2str("%Y", time());
if ( $F::includeCurrentMonthP eq "no" ) {
	$lastMonth--;
} elsif ( $F::includeCurrentMonthP ne "yes" ) {
	die "invalid value for includeCurrentMonthP parameter supplied";
}

# convert the number of months to subtract into years & months
#
die "invalid number of months specified" if $F::months < 1;
$F::months--;	# since we're using this for subtraction
my $years = int ($F::months / 12); 
my $months = $F::months % 12;

# subtract years and months to calculate our starting year and month
#
my $startYear = $lastYear - $years;
my $startMonth;
if ( $months < $lastMonth ) {
	$startMonth = $lastMonth - $months;
} else {
	$startYear--;
	$startMonth = 12 - ( $months - $lastMonth );
	die "Internal date calculation error" if ($startMonth < 0);
}

# init some variables
#
my $curYear=$startYear;
my $curMonth=$startMonth;
my $NetscapeSQL;
my $q;

# set up the appropriate SQL to describe what an "Netscape" checkin looks 
# like, depending on whether mozilla.org is considered Netscape
#
if ($F::mozillaOrgAsNscp eq "yes") {
	$NetscapeSQL = ' regexp "[@%]netscape\\.com|[@%]mozilla\\.org"';
} elsif ($F::mozillaOrgAsNscp eq "no" ) {
	$NetscapeSQL = ' regexp "[@%]netscape\\.com"';
} else {
	die ("Internal error: mozillaOrgAsNscp not set");
}

print header();
print start_html("-title"=>'mozilla.org statistics',
			-BGCOLOR=>'#FFFFFF',
			-TEXT=>'#000000');

# do the stat generation
#
for ( ; $curYear <= $lastYear ; $curYear++ ) {

	for ( ; ( $curYear==$lastYear && $curMonth<=$lastMonth ) 
		|| ($curYear!=$lastYear && $curMonth<13); $curMonth++ ) {

	   # generate each stat for the month in question
	   #
	   foreach ( 0 .. $#::STATINFO ) {
	
		# search the appropriate db
		#
		if (! $::db->selectdb($::STATINFO[$_][1]) ) {
			die "MySQL returned \"$Mysql::db_errstr\" while " .
				"attempting to selectdb(\"$::STATINFO[$_][1]" .
				"\") for the report \"$::STATINFO[$_][0]\"";
		}

		if (! $F::debugHtml ) { 
	
			# run the query
			#
			$q = $::db->query($::STATINFO[$_][4] . ' and ' . 
			 $::STATINFO[$_][3] . ' not ' . $NetscapeSQL . ' and ' 
                         . 'YEAR(' . $::STATINFO[$_][2] . ")=$curYear and "
			 . "MONTH(". $::STATINFO[$_][2] . ")=$curMonth");

			die "MySQL returned \"$Mysql::db_errstr\"" if (!$q);

			# push the stat onto the array
			#
			push @{$::STATS[$_]}, $q->numrows();
		} else {
			push @{$::STATS[$_]}, 0;
		}

		# show some progress, so the web server doesn't kill us for
		# inaction
		#
		print " ";

	   }
	}

	# if we finished the year, reset the current month to jan
	#
	$curMonth = 1;
}

# statistics output
#
print p();
print "\nAll numbers on this page represent the work done by Mozilla " .
	 "contributors other than Netscape.";

print p();
print "Though we don't generally think in these terms (a contribution is" .
      " a contribution, regardless of the source), there has been " .
      " some interest in these numbers.";

print p();
print "These statistics were generated using the assumption that " .
		" contributions from mozilla.org should " .
	        b( $F::mozillaOrgAsNscp eq "yes" ? "" : "not ") .  
	        "be considered Netscape contributions.";

print p();
print "Perhaps you are looking for " . 
      qq|<a href="http://mothra.mozilla.org/webtools/miscstats/">the Bugzilla statistics?</a> |;


# generate the row of headers listing the months
#
my @ROW = ();

# put in a dummy label, so that the labels start up aligned at correct column
#
push @ROW, "";

# here are the month names
#
my @MONTHS = ("Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep",
	     "Oct", "Nov", "Dec");

# make sure the first label mentions what year we're looking at
#
push @ROW, $MONTHS[$startMonth-1] . " $startYear";

# generate the labels
#
$curYear=$startYear;
$curMonth=$startMonth+1;

for ( ; $curYear <= $lastYear ; $curYear++ ) {
	for ( ; ( $curYear==$lastYear && $curMonth<=$lastMonth ) 
		|| ($curYear!=$lastYear && $curMonth<13); $curMonth++ ) {
		if ($curMonth != 1) {
			push @ROW, $MONTHS[$curMonth-1];
		} else { 
			push @ROW, $MONTHS[$curMonth-1] . " $curYear";
		}
	}

	# if we finished out the year, reset the month to january
	#
	$curMonth = 1;
}

# if we have an incomplete month, mark it as such
#
if ($F::includeCurrentMonthP eq "yes") {
	$ROW[$#ROW] .= div({"-style"=>"Color: red;"}, " (incomplete)");
}

# set up the table
#
my @ROWS;

# put the list of months on top
#
push @ROWS, Tr(th(\@ROW));

# stuff the stats into the table and print it;
#
foreach ( 0..$#::STATINFO ) {
	push @ROWS, th($::STATINFO[$_][0]).td($::STATS[$_]);
}

# have to workaround some annoying warning that happens inside of table()
#
$SIG{__WARN__} = \&squelchWarning;
print table({-border=>undef},Tr(\@ROWS));
delete $SIG{__WARN__};

print p();
print "Statistics generated at " . time2str("%c", time()) . ".";

print p(); 
print a({href=>"genstats.cgi"}, 
	"Tweak parameters & regenerate statistics (slow)");

print end_html();
exit 0;

sub squelchWarning {
}

