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
#                 Dan Mosedale <dmose@mozilla.org>

use diagnostics;
use strict;

require "CGI.pl";

# Shut up misguided -w warnings about "used only once".  For some reason,
# "use vars" chokes on me when I try it here.

sub sillyness {
    my $zz;
    $zz = $::buffer;
    $zz = %::COOKIE;
}
confirm_login();

print "Set-Cookie: PLATFORM=$::FORM{'product'} ; path=/ ; expires=Sun, 30-Jun-2029 00:00:00 GMT\n";
print "Set-Cookie: VERSION-$::FORM{'product'}=$::FORM{'version'} ; path=/ ; expires=Sun, 30-Jun-2029 00:00:00 GMT\n";
print "Content-type: text/html\n\n";

if (defined $::FORM{'maketemplate'}) {
    print "<TITLE>Bookmarks are your friend.</TITLE>\n";
    print "<H1>Template constructed.</H1>\n";
    
    my $url = "enter_bug.cgi?$::buffer";

    print "If you put a bookmark <a href=\"$url\">to this link</a>, it will\n";
    print "bring up the submit-a-new-bug page with the fields initialized\n";
    print "as you've requested.\n";
    PutFooter();
    exit;
}

PutHeader("Posting Bug -- Please wait", "Posting Bug", "One moment please...");

umask 0;
ConnectToDatabase();

if (!defined $::FORM{'component'} || $::FORM{'component'} eq "") {
    print "You must choose a component that corresponds to this bug.  If\n";
    print "necessary, just guess.  But please hit the <B>Back</B> button\n";
    print "and choose a component.\n";
    PutFooter();
    exit 0
}

if (!defined $::FORM{'short_desc'} || trim($::FORM{'short_desc'}) eq "") {
    print "You must enter a summary for this bug.  Please hit the\n";
    print "<B>Back</B> button and try again.\n";
    PutFooter();
    exit;
}

if ( Param("strictvaluechecks") ) {
    GetVersionTable();  
    CheckFormField(\%::FORM, 'reporter');
    CheckFormField(\%::FORM, 'product', \@::legal_product);
    CheckFormField(\%::FORM, 'version', \@{$::versions{$::FORM{'product'}}});
    CheckFormField(\%::FORM, 'rep_platform', \@::legal_platform);
    CheckFormField(\%::FORM, 'bug_severity', \@::legal_severity);
    CheckFormField(\%::FORM, 'priority', \@::legal_priority);
    CheckFormField(\%::FORM, 'op_sys', \@::legal_opsys);
    CheckFormFieldDefined(\%::FORM, 'assigned_to');
    CheckFormField(\%::FORM, 'bug_status', \@::legal_bug_status);
    CheckFormFieldDefined(\%::FORM, 'bug_file_loc');
    CheckFormField(\%::FORM, 'component', 
                   \@{$::components{$::FORM{'product'}}});
    CheckFormFieldDefined(\%::FORM, 'comment');
}

my $forceAssignedOK = 0;
if ($::FORM{'assigned_to'} eq "") {
    SendSQL("select initialowner from components where program=" .
            SqlQuote($::FORM{'product'}) .
            " and value=" . SqlQuote($::FORM{'component'}));
    $::FORM{'assigned_to'} = FetchOneColumn();
    $forceAssignedOK = 1;
}

$::FORM{'assigned_to'} = DBNameToIdAndCheck($::FORM{'assigned_to'}, $forceAssignedOK);
$::FORM{'reporter'} = DBNameToIdAndCheck($::FORM{'reporter'});


my @bug_fields = ("reporter", "product", "version", "rep_platform",
                  "bug_severity", "priority", "op_sys", "assigned_to",
                  "bug_status", "bug_file_loc", "short_desc", "component");

if (Param("useqacontact")) {
    SendSQL("select initialqacontact from components where program=" .
            SqlQuote($::FORM{'product'}) .
            " and value=" . SqlQuote($::FORM{'component'}));
    my $qacontact = FetchOneColumn();
    if (defined $qacontact && $qacontact ne "") {
        $::FORM{'qa_contact'} = DBNameToIdAndCheck($qacontact, 1);
        push(@bug_fields, "qa_contact");
    }
}



my @used_fields;
foreach my $f (@bug_fields) {
    if (exists $::FORM{$f}) {
        push (@used_fields, $f);
    }
}

my $query = "insert into bugs (\n" . join(",\n", @used_fields) . ",
creation_ts )
values (
";


foreach my $field (@used_fields) {
    $query .= SqlQuote($::FORM{$field}) . ",\n";
}

my $comment = $::FORM{'comment'};
$comment =~ s/\r\n/\n/g;     # Get rid of windows-style line endings.
$comment =~ s/\r/\n/g;       # Get rid of mac-style line endings.
$comment = trim($comment);

$query .= "now())\n";


my %ccids;


if (defined $::FORM{'cc'}) {
    foreach my $person (split(/[ ,]/, $::FORM{'cc'})) {
        if ($person ne "") {
            $ccids{DBNameToIdAndCheck($person)} = 1;
        }
    }
}


# print "<PRE>$query</PRE>\n";

SendSQL($query);

SendSQL("select LAST_INSERT_ID()");
my $id = FetchOneColumn();

SendSQL("INSERT INTO longdescs (bug_id, who, bug_when, thetext) VALUES " .
        "($id, $::FORM{'reporter'}, now(), " . SqlQuote($comment) . ")");

foreach my $person (keys %ccids) {
    SendSQL("insert into cc (bug_id, who) values ($id, $person)");
}

print "<TABLE BORDER=1><TD><H2>Bug $id posted</H2>\n";
system("./processmail $id $::COOKIE{'Bugzilla_login'}");
print "<TD><A HREF=\"show_bug.cgi?id=$id\">Back To BUG# $id</A></TABLE>\n";

print "<BR><A HREF=\"createattachment.cgi?id=$id\">Attach a file to this bug</a>\n";

navigation_header();

PutFooter();
exit;
