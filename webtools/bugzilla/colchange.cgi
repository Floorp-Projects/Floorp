#!/usr/bonsaitools/bin/perl -wT
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

use lib qw(.);

use vars qw(
  @legal_keywords
  $buffer
  $template
  $vars
);

require "CGI.pl";

ConnectToDatabase();
quietly_check_login();

print "Content-type: text/html\n";

# The master list not only says what fields are possible, but what order
# they get displayed in.

GetVersionTable();

my @masterlist = ("opendate", "changeddate", "severity", "priority",
                  "platform", "owner", "reporter", "status", "resolution",
                  "product", "component", "version", "os", "votes");

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

$vars->{masterlist} = \@masterlist;

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

$vars->{collist} = \@collist;

$vars->{splitheader} = 0;
if ($::COOKIE{'SPLITHEADER'}) {
    $vars->{splitheader} = 1;
}

my %desc = ();
foreach my $i (@masterlist) {
    $desc{$i} = $i;
}

$desc{'summary'} = "Summary (first 60 characters)";
$desc{'summaryfull'} = "Full Summary";

$vars->{desc} = \%desc;
$vars->{buffer} = $::buffer;

# Generate and return the UI (HTML page) from the appropriate template.
print "Content-type: text/html\n\n";
$template->process("list/change-columns.html.tmpl", $vars)
  || ThrowTemplateError($template->error());

