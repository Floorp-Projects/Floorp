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

print "Content-type: text/html\n";
print "\n";

ConnectToDatabase();


sub Status {
    my ($str) = (@_);
    print "$str <P>\n";
}

sub Alert {
    my ($str) = (@_);
    Status("<font color=red>$str</font>");
}

sub BugLink {
    my ($id) = (@_);
    return "<a href='show_bug.cgi?id=$id'>$id</a>";
}


PutHeader("Bugzilla Sanity Check");

print "OK, now running sanity checks.<P>\n";

Status("Checking profile ids...");

SendSQL("select userid,login_name from profiles");

my @row;

my %profid;

while (@row = FetchSQLData()) {
    my ($id, $email) = (@row);
    if ($email =~ /^[^@, ]*@[^@, ]*\.[^@, ]*$/) {
        $profid{$id} = 1;
    } else {
        Alert "Bad profile id $id &lt;$email&gt;."
    }
}


undef $profid{0};


Status("Checking reporter/assigned_to ids");
SendSQL("select bug_id,reporter,assigned_to from bugs");

my %bugid;

while (@row = FetchSQLData()) {
    my($id, $reporter, $assigned_to) = (@row);
    $bugid{$id} = 1;
    if (!defined $profid{$reporter}) {
        Alert("Bad reporter $reporter in " . BugLink($id));
    }
    if (!defined $profid{$assigned_to}) {
        Alert("Bad assigned_to $assigned_to in" . BugLink($id));
    }
}

Status("Checking CC table");

SendSQL("select bug_id,who from cc");
while (@row = FetchSQLData()) {
    my ($id, $cc) = (@row);
    if (!defined $profid{$cc}) {
        Alert("Bad cc $cc in " . BugLink($id));
    }
}


Status("Checking activity table");

SendSQL("select bug_id,who from bugs_activity");

while (@row = FetchSQLData()) {
    my ($id, $who) = (@row);
    if (!defined $bugid{$id}) {
        Alert("Bad bugid " . BugLink($id));
    }
    if (!defined $profid{$who}) {
        Alert("Bad who $who in " . BugLink($id));
    }
}
