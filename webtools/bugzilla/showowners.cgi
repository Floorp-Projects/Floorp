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
# Contributor(s): Bryce Nesbitt <bryce@nextbus.com>
#
# This program lists all BugZilla users, and lists what modules they
# either own or are default QA for.  It is very slow on large databases.

use diagnostics;
use strict;

require "CGI.pl";
require "globals.pl";

# Fetch, one row at a time, the product and module.
# Build the contents of the table cell listing each unique
# product just once, with all the modules.
sub FetchAndFormat {
	my $result = "";
	my $temp = "";
	my @row = "";

	while (@row = FetchSQLData()) {
		if( $temp ne $row[0] ) {
			$result .= " " . $row[0] . ": ";
		} else {
			$result .= ", ";
		}
		$temp = $row[0];
		$result .=  "<I>" . $row[1] .  "</I>";
	}
	return( $result );
}


# Start the resulting web page
print "Content-type: text/html\n\n";
print "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\">
<html><head><title>BugZilla module owners list</title></head>\n";

ConnectToDatabase();
GetVersionTable();

# Collect all BugZilla user names
SendSQL("select login_name,userid from profiles order by login_name");
my @list;
my @row;
while (@row = FetchSQLData()) {
	push @list, $row[0];
}

print "<P>The following is a list of BugZilla users who are the default owner
for at least one module.  BugZilla will only assign or Cc: a bug to the exact
name stored in the database.  Click on a name to see bugs assigned to that person:</P>\n";
print "<table border=1>\n";
print "<tr><td><B>Login name</B></td>\n";
print "<td><B>Default owner for</B></td><td><B>Default QA for</B></td>\n";

# If a user is a initialowner or initialqacontact, list their modules
my $person;
my $nospamperson;
my $firstcell;
my $secondcell;
my @nocell;
foreach $person (@list) {

	my $qperson = SqlQuote($person);

	SendSQL("select program,value from components\
		 where initialowner = $qperson order by program,value");
	$firstcell = FetchAndFormat();

	SendSQL("select program,value from components\
		 where initialqacontact = $qperson order by program,value");
	$secondcell = FetchAndFormat();

	$_ = $person;		# Anti-spam
	s/@/ @/;		# Mangle
	$nospamperson = $_;	# Email 

	if( $firstcell || $secondcell ) {
		print "<tr>";

		print "<td>\n";
		print "<a href=\"buglist.cgi?bug_status=NEW&bug_status=ASSIGNED&bug_status=REOPENED&email1=${person}&emailtype1=substring&emailassigned_to1=1&cmdtype=doit&newqueryname=&order=%22Importance%22&form_name=query\">${nospamperson}</a>\n";
		print "</td>\n";

		print "<td>";
		print $firstcell;
		print "</td>\n";

		print "<td>";
		print $secondcell;
		print "</td>\n";

		print "</tr>\n";
	} else {
		push @nocell, $person;
	}
}

print "<tr>";
print "<td colspan=3>";
print "Other valid logins: ";
foreach $person (@nocell) {
	$_ = $person;		# Anti-spam
	s/@/ @/;		# Mangle
	$nospamperson = $_;	# Email 

	print "<a href=\"buglist.cgi?bug_status=NEW&bug_status=ASSIGNED&bug_status=REOPENED&email1=${person}&emailtype1=substring&emailassigned_to1=1&cmdtype=doit&newqueryname=&order=%22Importance%22&form_name=query\">${nospamperson}</a>\n";
	print ", ";
}
print "</td>";
print "</tr>\n";

print "</table>\n";

# Enhancement ideas
#	o Use just one table cell for each person.  The table gets unbalanced for installs
#	  where just a few QA people handle lots of modules
#	o Optimize for large systems.  Terry notes:
# 	  The problem is that you go query the components table 10,000 times, 
#	  twice for each of the 5,000 logins that we have.  Yow!
#	 
#	  It would be better to generate your initial list of logins by selecting 
#	  for distinct initialqacontact and initialowner values from the 
#	  components database.  Then when you generate the list of "other 
#	  logins", you can query for the whole list of logins and subtract out 
#	  things that were in the components database.
