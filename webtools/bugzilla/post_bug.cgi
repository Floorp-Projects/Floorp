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
#                 Joe Robins <jmrobins@tgix.com>

use diagnostics;
use strict;

require "CGI.pl";

# Shut up misguided -w warnings about "used only once".  For some reason,
# "use vars" chokes on me when I try it here.

sub sillyness {
    my $zz;
    $zz = $::buffer;
    $zz = $::usergroupset;
    $zz = %::COOKIE;
    $zz = %::components;
    $zz = %::versions;
    $zz = @::legal_bug_status;
    $zz = @::legal_opsys;
    $zz = @::legal_platform;
    $zz = @::legal_priority;
    $zz = @::legal_product;
    $zz = @::legal_severity;
    $zz = %::target_milestone;
}

confirm_login();

my $cookiepath = Param("cookiepath");
print "Set-Cookie: PLATFORM=$::FORM{'product'} ; path=$cookiepath ; expires=Sun, 30-Jun-2029 00:00:00 GMT\n" if ( exists $::FORM{'product'} );
print "Set-Cookie: VERSION-$::FORM{'product'}=$::FORM{'version'} ; path=$cookiepath ; expires=Sun, 30-Jun-2029 00:00:00 GMT\n" if ( exists $::FORM{'product'} && exists $::FORM{'version'} );

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

my $product = $::FORM{'product'};

if(Param("usebuggroupsentry") && GroupExists($product)) {
  if(!UserInGroup($product)) {
    print "<H1>Permission denied.</H1>\n";
    print "Sorry; you do not have the permissions necessary to enter\n";
    print "a bug against this product.\n";
    print "<P>\n";
    PutFooter();
    exit;
  }
}

if (!defined $::FORM{'component'} || $::FORM{'component'} eq "") {
    PuntTryAgain("You must choose a component that corresponds to this bug. " .
                 "If necessary, just guess.");
}

if (!defined $::FORM{'short_desc'} || trim($::FORM{'short_desc'}) eq "") {
    PuntTryAgain("You must enter a summary for this bug.");
}

if ($::FORM{'assigned_to'} eq "") {
    SendSQL("select initialowner from components where program=" .
            SqlQuote($::FORM{'product'}) .
            " and value=" . SqlQuote($::FORM{'component'}));
    $::FORM{'assigned_to'} = FetchOneColumn();
} else {
    $::FORM{'assigned_to'} = DBNameToIdAndCheck($::FORM{'assigned_to'});
}

$::FORM{'reporter'} = DBNameToIdAndCheck($::FORM{'reporter'});


my @bug_fields = ("reporter", "product", "version", "rep_platform",
                  "bug_severity", "priority", "op_sys", "assigned_to",
                  "bug_status", "bug_file_loc", "short_desc", "component",
                  "target_milestone");

if (Param("useqacontact")) {
    SendSQL("select initialqacontact from components where program=" .
            SqlQuote($::FORM{'product'}) .
            " and value=" . SqlQuote($::FORM{'component'}));
    my $qacontact = FetchOneColumn();
    if (defined $qacontact && $qacontact != 0) {
        $::FORM{'qa_contact'} = $qacontact;
        push(@bug_fields, "qa_contact");
    }
}

if (exists $::FORM{'bug_status'}) {
    if (!UserInGroup("canedit") && !UserInGroup("canconfirm")) {
        delete $::FORM{'bug_status'};
    }
}

if (!exists $::FORM{'bug_status'}) {
    $::FORM{'bug_status'} = $::unconfirmedstate;
    SendSQL("SELECT votestoconfirm FROM products WHERE product = " .
            SqlQuote($::FORM{'product'}));
    if (!FetchOneColumn()) {
        $::FORM{'bug_status'} = "NEW";
    }
}

if (!exists $::FORM{'target_milestone'}) {
    SendSQL("SELECT defaultmilestone FROM products " .
            "WHERE product = " . SqlQuote($::FORM{'product'}));
    $::FORM{'target_milestone'} = FetchOneColumn();
}

if ( Param("strictvaluechecks") ) {
    GetVersionTable();  
    CheckFormField(\%::FORM, 'reporter');
    CheckFormField(\%::FORM, 'product', \@::legal_product);
    CheckFormField(\%::FORM, 'version', \@{$::versions{$::FORM{'product'}}});
    CheckFormField(\%::FORM, 'target_milestone',
                   \@{$::target_milestone{$::FORM{'product'}}});
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

my @used_fields;
foreach my $f (@bug_fields) {
    if (exists $::FORM{$f}) {
        push (@used_fields, $f);
    }
}
if (exists $::FORM{'bug_status'} && $::FORM{'bug_status'} ne $::unconfirmedstate) {
    push(@used_fields, "everconfirmed");
    $::FORM{'everconfirmed'} = 1;
}

my $query = "INSERT INTO bugs (\n" . join(",\n", @used_fields) . ",
creation_ts, groupset)
VALUES (
";

foreach my $field (@used_fields) {
# fix for 42609. if there is a http:// only in bug_file_loc, strip
# it out and send an empty value. 
    if ($field eq 'bug_file_loc') {
       if ($::FORM{$field} eq 'http://') {
           $::FORM{$field} = "";
           $query .= SqlQuote($::FORM{$field}) . ",\n";
           next;
       } 
       else {
          $query .= SqlQuote($::FORM{$field}) . ",\n";
       }
    }
    else {
       $query .= SqlQuote($::FORM{$field}) . ",\n";
    }
}

my $comment = $::FORM{'comment'};
$comment =~ s/\r\n/\n/g;     # Get rid of windows-style line endings.
$comment =~ s/\r/\n/g;       # Get rid of mac-style line endings.
$comment = trim($comment);
# If comment is all whitespace, it'll be null at this point.  That's
# OK except for the fact that it causes e-mail to be suppressed.
$comment = $comment ? $comment : " ";

$query .= "now(), (0";

foreach my $b (grep(/^bit-\d*$/, keys %::FORM)) {
    if ($::FORM{$b}) {
        my $v = substr($b, 4);
        $v =~ /^(\d+)$/
          || PuntTryAgain("One of the group bits submitted was invalid.");
        if (!GroupIsActive($v)) {
            # Prevent the user from adding the bug to an inactive group.
            # Should only happen if there is a bug in Bugzilla or the user
            # hacked the "enter bug" form since otherwise the UI 
            # for adding the bug to the group won't appear on that form.
            PuntTryAgain("You can't add this bug to the inactive group " . 
                         "identified by the bit '$v'. This shouldn't happen, " . 
                         "so it may indicate a bug in Bugzilla.");
        }
        $query .= " + $v";    # Carefully written so that the math is
                                # done by MySQL, which can handle 64-bit math,
                                # and not by Perl, which I *think* can not.
    }
}



$query .= ") & $::usergroupset)\n";


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
system("./processmail", $id, $::COOKIE{'Bugzilla_login'});
print "<TD><A HREF=\"show_bug.cgi?id=$id\">Back To BUG# $id</A></TABLE>\n";

print "<BR><A HREF=\"createattachment.cgi?id=$id\">Attach a file to this bug</a>\n";

navigation_header();

PutFooter();
exit;
