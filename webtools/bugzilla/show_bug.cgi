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
# Contributor(s): Terry Weissman <terry@mozilla.org>

use diagnostics;
use strict;

require "CGI.pl";

ConnectToDatabase();

if ($::FORM{'GoAheadAndLogIn'}) {
    confirm_login();
} else {
    quietly_check_login();
}

######################################################################
# Begin Data/Security Validation
######################################################################

# Make sure the bug ID is a positive integer representing an existing
# bug that the user is authorized to access.
if (defined ($::FORM{'id'})) {
    ValidateBugID($::FORM{'id'});
}

######################################################################
# End Data/Security Validation
######################################################################

print "Content-type: text/html\n";
print "\n";

if (!defined $::FORM{'id'}) {
    PutHeader("Search by bug number");
    print "<FORM METHOD=GET ACTION=\"show_bug.cgi\">\n";
    print "You may find a single bug by entering its bug id here: \n";
    print "<INPUT NAME=id>\n";
    print "<INPUT TYPE=\"submit\" VALUE=\"Show Me This Bug\">\n";
    print "</FORM>\n";
    PutFooter();
    exit;
}

GetVersionTable();

# Get the bug's summary (short description) and display it as
# the page title.
SendSQL("SELECT short_desc FROM bugs WHERE bug_id = $::FORM{'id'}");
my ($summary) = FetchSQLData();
$summary = html_quote($summary);
PutHeader("Bug $::FORM{'id'} - $summary", "Bugzilla Bug $::FORM{'id'}", $summary );

navigation_header();

print "<HR>\n";

$! = 0;
do "bug_form.pl" || die "Error doing bug_form.pl: $!";
