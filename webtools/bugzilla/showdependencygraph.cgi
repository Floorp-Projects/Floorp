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

quietly_check_login();

# More warning suppression silliness.
$::userid = $::userid;
$::usergroupset = $::usergroupset;

######################################################################
# Begin Data/Security Validation
######################################################################

# Make sure the bug ID is a positive integer representing an existing
# bug that the user is authorized to access.
if (defined $::FORM{'id'}) {
  ValidateBugID($::FORM{'id'});
}

######################################################################
# End Data/Security Validation
######################################################################

my $id = $::FORM{'id'};

my $urlbase = Param("urlbase");

my %seen;
my %edgesdone;

sub AddLink {
    my ($blocked, $dependson) = (@_);
    my $key = "$blocked,$dependson";
    if (!exists $edgesdone{$key}) {
        $edgesdone{$key} = 1;
        print DOT "$blocked -> $dependson\n";
        $seen{$blocked} = 1;
        $seen{$dependson} = 1;
    }
}


print "Content-type: text/html\n\n";
PutHeader("Dependency graph", "Dependency graph", $id);

$::FORM{'rankdir'} = "LR" if !defined $::FORM{'rankdir'};


if (defined $id) {
    mkdir("data/webdot", 0777);

    my $filename = "data/webdot/$$.dot";
    open(DOT, ">$filename") || die "Can't create $filename";
    print DOT "digraph G {";
    print DOT qq{
graph [URL="${urlbase}query.cgi", rankdir=$::FORM{'rankdir'}, size="64,64"]
node [URL="${urlbase}show_bug.cgi?id=\\N", style=filled, color=lightgrey]
};
    my %baselist;

    foreach my $i (split('[\s,]+', $::FORM{'id'})) {
        $i = trim($i);
        if ($i ne "") {
            $baselist{$i} = 1;
        }
    }
    my @basearray = keys(%baselist);

    if ($::FORM{'doall'}) {
        SendSQL("select blocked, dependson from dependencies");
        
        while (MoreSQLData()) {
            my ($blocked, $dependson) = (FetchSQLData());
            AddLink($blocked, $dependson);
        }
    } else {
        my @stack = @basearray;
        while (@stack) {
            my $id = shift @stack;
            SendSQL("select blocked, dependson from dependencies where blocked = $id or dependson = $id");
            while (MoreSQLData()) {
                my ($blocked, $dependson) = (FetchSQLData());
                if ($blocked != $id && !exists $seen{$blocked}) {
                    push @stack, $blocked;
                }
                if ($dependson != $id && !exists $seen{$dependson}) {
                    push @stack, $dependson;
                }
                AddLink($blocked, $dependson);
            }
        }
    }

    foreach my $k (@basearray) {
        $seen{$k} = 1;
    }
    foreach my $k (keys(%seen)) {
        my $summary = "";
        my $stat;
        if ($::FORM{'showsummary'}) {
            SendSQL(SelectVisible("select bug_status, short_desc from bugs where bug_id = $k",
                                  $::userid,
                                  $::usergroupset));
            ($stat, $summary) = (FetchSQLData());
            $stat = "NEW" if !defined $stat;
            $summary = "" if !defined $summary;
        } else {
            SendSQL("select bug_status from bugs where bug_id = $k");
            $stat = FetchOneColumn();
        }
        my @params;
#        print DOT "$k [URL" . qq{="${urlbase}show_bug.cgi?id=$k"};
        if ($summary ne "") {
            $summary =~ s/([\\\"])/\\$1/g;
            push(@params, qq{label="$k\\n$summary"});
        }
        if (exists $baselist{$k}) {
            push(@params, "shape=box");
        }
        my $opened = ($stat eq "NEW" || $stat eq "ASSIGNED" ||
                      $stat eq "REOPENED");
        if ($opened) {
            push(@params, "color=green");
        }
        if (@params) {
            print DOT "$k [" . join(',', @params) . "]\n";
        } else {
            print DOT "$k\n";
        }
    }


    print DOT "}\n";
    close DOT;
    chmod 0777, $filename;
    
    my $url = PerformSubsts(Param("webdotbase")) . $filename;

    print qq{<a href="$url.map"> <img src="$url.gif" ismap> </a><hr>\n};

    # Cleanup any old .dot files created from previous runs.
    my $since = time() - 24 * 60 * 60;
    foreach my $f (glob("data/webdot/*.dot")) {
        if (ModTime($f) < $since) {
            unlink $f;
        }
    }
} else {
    $::FORM{'id'} = "";
    $::FORM{'doall'} = 0;
    $::FORM{'showsummary'} = 0;
}    

print "
<form>
<table>
<tr>
<th align=right>Bug numbers:</th>
<td><input name=id value=\"" . value_quote($::FORM{'id'}) . "\"></td>
<td><input type=checkbox name=doall" . ($::FORM{'doall'} ? " checked" : "") .
">Show <b>every</b> bug in the system with 
dependencies</td>
</tr>
<tr><td colspan=3><input type=checkbox name=showsummary" .
($::FORM{'showsummary'} ? " checked" : "") . ">Show the summary of all bugs
</tr>
<tr><td colspan=3><select name=rankdir>
<option value=\"TB\"" . ($::FORM{'rankdir'} eq 'TB' ? 'selected' : '') .
">Orient top-to-bottom
<option value=\"LR\"" . ($::FORM{'rankdir'} eq 'LR' ? 'selected' : '') .
">Orient left-to-right
</select></td></tr>
</table>
<input type=submit value=\"Submit\">
</form>
 ";

PutFooter();
