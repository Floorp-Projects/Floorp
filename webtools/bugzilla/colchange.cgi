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

sub sillyness { # shut up "used only once" warnings
  my $zz = @::legal_keywords;
}

require "CGI.pl";

print "Content-type: text/html\n";

# The master list not only says what fields are possible, but what order
# they get displayed in.

ConnectToDatabase();
GetVersionTable();

my @masterlist = ("opendate", "changeddate", "severity", "priority",
                  "platform", "owner", "reporter", "status", "resolution",
                  "component", "product", "version", "os", "votes");

if (Param("usetargetmilestone")) {
    push(@masterlist, "target_milestone");
}
if (Param("useqacontact")) {
    push(@masterlist, "qa_contact");
}
if (Param("usestatuswhiteboard")) {
    push(@masterlist, "status_whiteboard");
}
if (@::legal_keywords) {
    push(@masterlist, "keywords");
}


push(@masterlist, ("summary", "summaryfull"));


my @collist;
if (defined $::FORM{'rememberedquery'}) {
    my $splitheader = 0;
    if (defined $::FORM{'resetit'}) {
        @collist = @::default_column_list;
    } else {
        foreach my $i (@masterlist) {
            if (defined $::FORM{"column_$i"}) {
                push @collist, $i;
            }
        }
        if (exists $::FORM{'splitheader'}) {
            $splitheader = $::FORM{'splitheader'};
        }
    }
    my $list = join(" ", @collist);
    my $urlbase = Param("urlbase");
    my $cookiepath = Param("cookiepath");
    print "Set-Cookie: COLUMNLIST=$list ; path=$cookiepath ; expires=Sun, 30-Jun-2029 00:00:00 GMT\n";
    print "Set-Cookie: SPLITHEADER=$::FORM{'splitheader'} ; path=$cookiepath ; expires=Sun, 30-Jun-2029 00:00:00 GMT\n";
    print "Refresh: 0; URL=buglist.cgi?$::FORM{'rememberedquery'}\n";
    print "\n";
    print "<META HTTP-EQUIV=Refresh CONTENT=\"1; URL=$urlbase"."buglist.cgi?$::FORM{'rememberedquery'}\">\n";
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

my $splitheader = 0;
if ($::COOKIE{'SPLITHEADER'}) {
    $splitheader = 1;
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
print "on submit.  (Cookies are required.)\n";
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
print BuildPulldown("splitheader",
                    [["0", "Normal headers (prettier)"],
                     ["1", "Stagger headers (often makes list more compact)"]],
                    $splitheader);
print "<P>\n";

print "<INPUT TYPE=\"submit\" VALUE=\"Submit\">\n";
print "</FORM>\n";
print "<FORM ACTION=colchange.cgi>\n";
print "<INPUT TYPE=HIDDEN NAME=rememberedquery VALUE=$::buffer>\n";
print "<INPUT TYPE=HIDDEN NAME=resetit VALUE=1>\n";
print "<INPUT TYPE=\"submit\" VALUE=\"Reset to Bugzilla default\">\n";
print "</FORM>\n";
PutFooter();
