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

use vars %::FORM;


my $id = $::FORM{'id'};
my $linkedid = qq{<a href="show_bug.cgi?id=$id">$id</a>};

print "Content-type: text/html\n\n";
PutHeader("Dependency tree", "Dependency tree", "Bug $linkedid");

ConnectToDatabase();

quietly_check_login();

$::usergroupset = $::usergroupset; # More warning suppression silliness.

my %seen;

sub DumpKids {
    my ($i, $me, $target) = (@_);
    if (exists $seen{$i}) {
        return;
    }
    $seen{$i} = 1;
    SendSQL("select $target from dependencies where $me = $i order by $target");
    my @list;
    while (MoreSQLData()) {
        push(@list, FetchOneColumn());
    }
    if (@list) {
        print "<ul>\n";
        foreach my $kid (@list) {
            SendSQL("select bug_id, bug_status, short_desc from bugs where bug_id = $kid and bugs.groupset & $::usergroupset = bugs.groupset");
            my ($bugid, $stat, $short_desc) = (FetchSQLData());
            if (!defined $bugid) {
                next;
            }
            my $opened = ($stat eq "NEW" || $stat eq "ASSIGNED" ||
                          $stat eq "REOPENED");
            print "<li>";
            if (!$opened) {
                print "<strike>";
            }
            $short_desc = html_quote($short_desc);
            print qq{<a href="show_bug.cgi?id=$kid">$kid $short_desc</a>};
            if (!$opened) {
                print "</strike>";
            }
            DumpKids($kid, $me, $target);
        }
        print "</ul>\n";
    }
}

print "<h1>Bugs that bug $linkedid depends on</h1>";
DumpKids($id, "blocked", "dependson");
print "<h1>Bugs that depend on bug $linkedid</h1>";
undef %seen;
DumpKids($id, "dependson", "blocked");

navigation_header();
