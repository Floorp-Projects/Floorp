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

# The master list not only says what fields are possible, but what order
# they get displayed in.

my @masterlist = ("opendate", "changeddate", "severity", "priority",
                  "platform", "owner", "reporter", "status", "resolution",
                  "component", "product", "version", "project", "os");

if (Param("usetargetmilestone")) {
    push(@masterlist, "target_milestone");
}
if (Param("useqacontact")) {
    push(@masterlist, "qa_contact");
}
if (Param("usestatuswhiteboard")) {
    push(@masterlist, "status_whiteboard");
}


push(@masterlist, ("summary", "summaryfull"));


my @collist;
if (defined $::FORM{'rememberedquery'}) {
    if (defined $::FORM{'resetit'}) {
        @collist = @::default_column_list;
    } else {
        foreach my $i (@masterlist) {
            if (defined $::FORM{"column_$i"}) {
                push @collist, $i;
            }
        }
    }
    my $list = join(" ", @collist);
    print "Set-Cookie: COLUMNLIST=$list ; path=/ ; expires=Sun, 30-Jun-2029 00:00:00 GMT\n";
    print "Refresh: 0; URL=buglist.cgi?$::FORM{'rememberedquery'}\n";
    print "\n";
    print "<TITLE>What a hack.</TITLE>\n";
    PutHeader ("Change columns");
    print "Resubmitting your query with new columns...\n";
    exit;
}

if (defined $::COOKIE{'COLUMNLIST'}) {
    @collist = split(/ /, $::COOKIE{'COLUMNLIST'});
} else {
    @collist = @::default_column_list;
}


my %desc;
foreach my $i (@masterlist) {
    $desc{$i} = $i;
}

$desc{'summary'} = "Summary (first 60 characters)";
$desc{'summaryfull'} = "Full Summary";


print "\n";
PutHeader ("Change columns");
print "Check which columns you wish to appear on the list, and then click\n";
print "on submit.\n";
print "<p>\n";
print "<FORM ACTION=colchange.cgi>\n";
print "<INPUT TYPE=HIDDEN NAME=rememberedquery VALUE=$::buffer>\n";

foreach my $i (@masterlist) {
    my $c;
    if (lsearch(\@collist, $i) >= 0) {
        $c = 'CHECKED';
    } else {
        $c = '';
    }
    print "<INPUT TYPE=checkbox NAME=column_$i $c>$desc{$i}<br>\n";
}
print "<P>\n";
print "<INPUT TYPE=\"submit\" VALUE=\"Submit\">\n";
print "</FORM>\n";
print "<FORM ACTION=colchange.cgi>\n";
print "<INPUT TYPE=HIDDEN NAME=rememberedquery VALUE=$::buffer>\n";
print "<INPUT TYPE=HIDDEN NAME=resetit VALUE=1>\n";
print "<INPUT TYPE=\"submit\" VALUE=\"Reset to Bugzilla default\">\n";
print "</FORM>\n";
