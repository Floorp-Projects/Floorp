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
# Contributor(s): Terry Weissman <terry@mozilla.org>

use diagnostics;
use strict;

require "CGI.pl";

# Shut up misguided -w warnings about "used only once":

use vars %::versions,
    %::components,
    %::COOKIE;

confirm_login();

print "Content-type: text/html\n\n";

GetVersionTable();

if ($::FORM{'product'} ne $::dontchange) {
    my $prod = $::FORM{'product'};
    my $vok = lsearch($::versions{$prod}, $::FORM{'version'}) >= 0;
    my $cok = lsearch($::components{$prod}, $::FORM{'component'}) >= 0;
    if (!$vok || !$cok) {
        print "<H1>Changing product means changing version and component.</H1>\n";
        print "You have chosen a new product, and now the version and/or\n";
        print "component fields are not correct.  (Or, possibly, the bug did\n";
        print "not have a valid component or version field in the first place.)\n";
        print "Anyway, please set the version and component now.<p>\n";
        print "<form>\n";
        print "<table>\n";
        print "<tr>\n";
        print "<td align=right><b>Product:</b></td>\n";
        print "<td>$prod</td>\n";
        print "</tr><tr>\n";
        print "<td align=right><b>Version:</b></td>\n";
        print "<td>" . Version_element($::FORM{'version'}, $prod) . "</td>\n";
        print "</tr><tr>\n";
        print "<td align=right><b>Component:</b></td>\n";
        print "<td>" . Component_element($::FORM{'component'}, $prod) . "</td>\n";
        print "</tr>\n";
        print "</table>\n";
        foreach my $i (keys %::FORM) {
            if ($i ne 'version' && $i ne 'component') {
                print "<input type=hidden name=$i value=\"" .
                value_quote($::FORM{$i}) . "\">\n";
            }
        }
        print "<input type=submit value=Commit>\n";
        print "</form>\n";
        print "</hr>\n";
        print "<a href=query.cgi>Cancel all this and go back to the query page.</a>\n";
        exit;
    }
}


my @idlist;
if (defined $::FORM{'id'}) {
    push @idlist, $::FORM{'id'};
} else {
    foreach my $i (keys %::FORM) {
        if ($i =~ /^id_/) {
            push @idlist, substr($i, 3);
        }
    }
}

if (!defined $::FORM{'who'}) {
    $::FORM{'who'} = $::COOKIE{'Bugzilla_login'};
}

print "<TITLE>Update Bug " . join(" ", @idlist) . "</TITLE>\n";
if (defined $::FORM{'id'}) {
    navigation_header();
}
print "<HR>\n";
$::query = "update bugs\nset";
$::comma = "";
umask(0);

sub DoComma {
    $::query .= "$::comma\n    ";
    $::comma = ",";
}

sub ChangeStatus {
    my ($str) = (@_);
    if ($str ne $::dontchange) {
        DoComma();
        $::query .= "bug_status = '$str'";
    }
}

sub ChangeResolution {
    my ($str) = (@_);
    if ($str ne $::dontchange) {
        DoComma();
        $::query .= "resolution = '$str'";
    }
}


my $foundbit = 0;
foreach my $b (grep(/^bit-\d*$/, keys %::FORM)) {
    if (!$foundbit) {
        $foundbit = 1;
        DoComma();
        $::query .= "groupset = 0";
    }
    if ($::FORM{$b}) {
        my $v = substr($b, 4);
        $::query .= "+ $v";     # Carefully written so that the math is
                                # done by MySQL, which can handle 64-bit math,
                                # and not by Perl, which I *think* can not.
    }
}


foreach my $field ("rep_platform", "priority", "bug_severity", "url",
                   "summary", "component", "bug_file_loc", "short_desc",
                   "product", "version", "component", "op_sys",
                   "target_milestone", "status_whiteboard") {
    if (defined $::FORM{$field}) {
        if ($::FORM{$field} ne $::dontchange) {
            DoComma();
            $::query .= "$field = " . SqlQuote(trim($::FORM{$field}));
        }
    }
}


if (defined $::FORM{'qa_contact'}) {
    my $name = trim($::FORM{'qa_contact'});
    if ($name ne $::dontchange) {
        my $id = 0;
        if ($name ne "") {
            $id = DBNameToIdAndCheck($name);
        }
        DoComma();
        $::query .= "qa_contact = $id";
    }
}


ConnectToDatabase();

SWITCH: for ($::FORM{'knob'}) {
    /^none$/ && do {
        last SWITCH;
    };
    /^accept$/ && do {
        ChangeStatus('ASSIGNED');
        last SWITCH;
    };
    /^clearresolution$/ && do {
        ChangeResolution('');
        last SWITCH;
    };
    /^resolve$/ && do {
        ChangeStatus('RESOLVED');
        ChangeResolution($::FORM{'resolution'});
        last SWITCH;
    };
    /^reassign$/ && do {
        ChangeStatus('NEW');
        DoComma();
        my $newid = DBNameToIdAndCheck($::FORM{'assigned_to'});
        $::query .= "assigned_to = $newid";
        last SWITCH;
    };
    /^reassignbycomponent$/ && do {
        if ($::FORM{'component'} eq $::dontchange) {
            print "You must specify a component whose owner should get\n";
            print "assigned these bugs.\n";
            exit 0
        }
        ChangeStatus('NEW');
        SendSQL("select initialowner from components where program=" .
                SqlQuote($::FORM{'product'}) . " and value=" .
                SqlQuote($::FORM{'component'}));
        my $newname = FetchOneColumn();
        my $newid = DBNameToIdAndCheck($newname, 1);
        DoComma();
        $::query .= "assigned_to = $newid";
        last SWITCH;
    };   
    /^reopen$/ && do {
        ChangeStatus('REOPENED');
        last SWITCH;
    };
    /^verify$/ && do {
        ChangeStatus('VERIFIED');
        last SWITCH;
    };
    /^close$/ && do {
        ChangeStatus('CLOSED');
        last SWITCH;
    };
    /^duplicate$/ && do {
        ChangeStatus('RESOLVED');
        ChangeResolution('DUPLICATE');
        my $num = trim($::FORM{'dup_id'});
        if ($num !~ /^[0-9]*$/) {
            print "You must specify a bug number of which this bug is a\n";
            print "duplicate.  The bug has not been changed.\n";
            exit;
        }
        if ($::FORM{'dup_id'} == $::FORM{'id'}) {
            print "Nice try.  But it doesn't really make sense to mark a\n";
            print "bug as a duplicate of itself, does it?\n";
            exit;
        }
        AppendComment($::FORM{'dup_id'}, $::FORM{'who'}, "*** Bug $::FORM{'id'} has been marked as a duplicate of this bug. ***");
        $::FORM{'comment'} .= "\n\n*** This bug has been marked as a duplicate of $::FORM{'dup_id'} ***";
        system("./processmail $::FORM{'dup_id'} < /dev/null > /dev/null 2> /dev/null &");
        last SWITCH;
    };
    # default
    print "Unknown action $::FORM{'knob'}!\n";
    exit;
}


if ($#idlist < 0) {
    print "You apparently didn't choose any bugs to modify.\n";
    print "<p>Click <b>Back</b> and try again.\n";
    exit;
}

if ($::comma eq "") {
    if (!defined $::FORM{'comment'} || $::FORM{'comment'} =~ /^\s*$/) {
        print "Um, you apparently did not change anything on the selected\n";
        print "bugs. <p>Click <b>Back</b> and try again.\n";
        exit
    }
}

my $basequery = $::query;

sub SnapShotBug {
    my ($id) = (@_);
    SendSQL("select " . join(',', @::log_columns) .
            " from bugs where bug_id = $id");
    return FetchSQLData();
}


foreach my $id (@idlist) {
    SendSQL("lock tables bugs write, bugs_activity write, cc write, profiles write");
    my @oldvalues = SnapShotBug($id);

    my $query = "$basequery\nwhere bug_id = $id";
    
# print "<PRE>$query</PRE>\n";

    if ($::comma ne "") {
        SendSQL($query);
    }
    
    if (defined $::FORM{'comment'}) {
        AppendComment($id, $::FORM{'who'}, $::FORM{'comment'});
    }
    
    if (defined $::FORM{'cc'} && ShowCcList($id) ne $::FORM{'cc'}) {
        my %ccids;
        foreach my $person (split(/[ ,]/, $::FORM{'cc'})) {
            if ($person ne "") {
                my $cid = DBNameToIdAndCheck($person);
                $ccids{$cid} = 1;
            }
        }
        
        SendSQL("delete from cc where bug_id = $id");
        foreach my $ccid (keys %ccids) {
            SendSQL("insert into cc (bug_id, who) values ($id, $ccid)");
        }
    }

    my @newvalues = SnapShotBug($id);
    my $whoid;
    my $timestamp;
    foreach my $col (@::log_columns) {
        my $old = shift @oldvalues;
        my $new = shift @newvalues;
        if (!defined $old) {
            $old = "";
        }
        if (!defined $new) {
            $new = "";
        }
        if ($old ne $new) {
            if (!defined $whoid) {
                $whoid = DBNameToIdAndCheck($::FORM{'who'});
                SendSQL("select delta_ts from bugs where bug_id = $id");
                $timestamp = FetchOneColumn();
            }
            if ($col eq 'assigned_to') {
                $old = DBID_to_name($old);
                $new = DBID_to_name($new);
            }
            $col = SqlQuote($col);
            $old = SqlQuote($old);
            $new = SqlQuote($new);
            my $q = "insert into bugs_activity (bug_id,who,when,field,oldvalue,newvalue) values ($id,$whoid,$timestamp,$col,$old,$new)";
            # puts "<pre>$q</pre>"
            SendSQL($q);
        }
    }
    
    print "<TABLE BORDER=1><TD><H1>Changes Submitted</H1>\n";
    print "<TD><A HREF=\"show_bug.cgi?id=$id\">Back To BUG# $id</A></TABLE>\n";

    SendSQL("unlock tables");

    system("./processmail $id < /dev/null > /dev/null 2> /dev/null &");
}

if (defined $::next_bug) {
    $::FORM{'id'} = $::next_bug;
    print "<HR>\n";

    navigation_header();
    do "bug_form.pl";
} else {
    print "<BR><A HREF=\"query.cgi\">Back To Query Page</A>\n";
}
