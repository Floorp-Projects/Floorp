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
# Contributor(s): Sam Ziegler <sam@ziegler.org>
# Terry Weissman <terry@mozilla.org>
# Mark Hamby <mhamby@logicon.com>

# Code derived from doeditcomponents.cgi


use diagnostics;
use strict;

require "CGI.pl";

confirm_login();

print "Content-type: text/html\n\n";

# foreach my $i (sort(keys %::FORM)) {
#     print value_quote("$i $::FORM{$i}") . "<BR>\n";
# }

if (!UserInGroup("editcomponents")) {
    print "<H1>Sorry, you aren't a member of the 'editcomponents' group.</H1>\n";
    print "And so, you aren't allowed to add components.\n";
    exit;
}


PutHeader("Adding new component");

unlink "data/versioncache";
GetVersionTable();

my $component = trim($::FORM{"component"});
my $product = trim($::FORM{"product"});
my $description = trim($::FORM{"description"});
my $initialowner = trim($::FORM{"initialowner"});
my $initialqacontact = trim($::FORM{"initialqacontact"});

if ($component eq "") {
    print "You must enter a name for the new component.  Please press\n";
    print "<b>Back</b> and try again.\n";
    exit;
}

# Check to ensure the component doesn't exist already.
SendSQL("SELECT value FROM components WHERE " .
      "program = " . SqlQuote($product) . " and " .
      "value = " . SqlQuote($component));
my @row = FetchSQLData();
if (@row) {
        print "<H1>Component already exists</H1>";
        print "The component '$component' already exists\n";
        print "for product '$product'.<P>\n";
        print "<p><a href=query.cgi>Go back to the query page</a>\n";
        exit;
}

# Check that the email addresses are legitimate.
foreach my $addr ($initialowner, $initialqacontact) {
    if ($addr ne "") {
        DBNameToIdAndCheck($addr);
    }
}

# Add the new component.
SendSQL("INSERT INTO components ( " .
      "value, program, description, initialowner, initialqacontact" .
      " ) VALUES ( " .
      SqlQuote($component) . "," .
      SqlQuote($product) . "," .
      SqlQuote($description) . "," .
      SqlQuote($initialowner) . "," .
      SqlQuote($initialqacontact) . ")" );

unlink "data/versioncache";

print "OK, done.<p>\n";
print "<a href=addcomponent.cgi>Edit another new component.</a><p>\n";
print "<a href=editcomponents.cgi>Edit existing components.</a><p>\n";
print "<a href=query.cgi>Go back to the query page.</a>\n";
