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
#                 Gervase Markham <gerv@gerv.net>

use strict;

use lib qw(.);

require "CGI.pl";

ConnectToDatabase();

quietly_check_login();

# Connect to the shadow database if this installation is using one to improve
# performance.
Bugzilla->instance->switch_to_shadow_db();

use vars qw($template $vars $userid);

my %seen;
my %edgesdone;

sub CreateImagemap {
    my $mapfilename = shift;
    my $map = "<map name=\"imagemap\">\n";
    my $default;

    open MAP, "<$mapfilename";
    while(my $line = <MAP>) {
        if($line =~ /^default ([^ ]*)(.*)$/) {
            $default = qq{<area alt="" shape="default" href="$1">\n};
        }
        if ($line =~ /^rectangle \((.*),(.*)\) \((.*),(.*)\) (http[^ ]*)(.*)?$/) {
            $map .= qq{<area alt="bug$6" name="bug$6" shape="rect" href="$5" coords="$1,$4,$3,$2">\n};
        }
    }
    close MAP;

    $map .= "$default</map>";
    return $map;
}

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

$::FORM{'rankdir'} = "LR" if !defined $::FORM{'rankdir'};

if (!defined($::FORM{'id'}) && !defined($::FORM{'doall'})) {
    ThrowCodeError("missing_bug_id");
}    

my $filename = "data/webdot/$$.dot";
my $urlbase = Param('urlbase');

open(DOT, ">$filename") || die "Can't create $filename";
print DOT "digraph G {";
print DOT qq{
graph [URL="${urlbase}query.cgi", rankdir=$::FORM{'rankdir'}, size="64,64"]
node [URL="${urlbase}show_bug.cgi?id=\\N", style=filled, color=lightgrey]
};

my %baselist;

if ($::FORM{'doall'}) {
    SendSQL("SELECT blocked, dependson FROM dependencies");

    while (MoreSQLData()) {
        my ($blocked, $dependson) = FetchSQLData();
        AddLink($blocked, $dependson);
    }
} else {
    foreach my $i (split('[\s,]+', $::FORM{'id'})) {
        $i = trim($i);
        ValidateBugID($i);
        $baselist{$i} = 1;
    }

    my @stack = keys(%baselist);
    foreach my $id (@stack) {
        SendSQL("SELECT blocked, dependson 
                 FROM   dependencies 
                 WHERE  blocked = $id or dependson = $id");
        while (MoreSQLData()) {
            my ($blocked, $dependson) = FetchSQLData();
            if ($blocked != $id && !exists $seen{$blocked}) {
                push @stack, $blocked;
            }

            if ($dependson != $id && !exists $seen{$dependson}) {
                push @stack, $dependson;
            }

            AddLink($blocked, $dependson);
        }
    }

    foreach my $k (keys(%baselist)) {
        $seen{$k} = 1;
    }
}

foreach my $k (keys(%seen)) {
    my $summary = "";
    my $stat;
    if ($::FORM{'showsummary'}) {
        if (CanSeeBug($k, $::userid)) {
            SendSQL("SELECT bug_status, short_desc FROM bugs " .
                                  "WHERE bugs.bug_id = $k");
            ($stat, $summary) = FetchSQLData();
            $stat = "NEW" if !defined $stat;
            $summary = "" if !defined $summary;
        }
    } else {
        SendSQL("SELECT bug_status FROM bugs WHERE bug_id = $k");
        $stat = FetchOneColumn();
    }
    my @params;

    if ($summary ne "") {
        $summary =~ s/([\\\"])/\\$1/g;
        push(@params, qq{label="$k\\n$summary"});
    }

    if (exists $baselist{$k}) {
        push(@params, "shape=box");
    }

    if ($stat =~ /^(NEW|ASSIGNED|REOPENED)$/) {
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

my $webdotbase = Param('webdotbase');

if ($webdotbase =~ /^https?:/) {
     # Remote dot server
     my $url = PerformSubsts($webdotbase) . $filename;
     $vars->{'image_url'} = $url . ".gif";
     $vars->{'map_url'} = $url . ".map";
} else {
    # Local dot installation
    my $pngfilename = "data/webdot/$$.png";
    my $mapfilename = "data/webdot/$$.map";
    system("$webdotbase","-Tpng","-o","$pngfilename","$filename");
    $vars->{'image_url'} = $pngfilename;
    system("$webdotbase","-Tismap","-o","$mapfilename","$filename");
    $vars->{'image_map'} = CreateImagemap($mapfilename);
}

# Cleanup any old .dot files created from previous runs.
my $since = time() - 24 * 60 * 60;
# Can't use glob, since even calling that fails taint checks for perl < 5.6
opendir(DIR, "data/webdot/");
my @files = grep { /\.dot$|\.png$|\.map$/ && -f "data/webdot/$_" } readdir(DIR);
closedir DIR;
foreach my $f (@files)
{
    $f = "data/webdot/$f";
    # Here we are deleting all old files. All entries are from the
    # data/webdot/ directory. Since we're deleting the file (not following
    # symlinks), this can't escape to delete anything it shouldn't
    trick_taint($f);
    if (ModTime($f) < $since) {
        unlink $f;
    }
}

$vars->{'bug_id'} = $::FORM{'id'};
$vars->{'multiple_bugs'} = ($::FORM{'id'} =~ /[ ,]/);
$vars->{'doall'} = $::FORM{'doall'};
$vars->{'rankdir'} = $::FORM{'rankdir'};
$vars->{'showsummary'} = $::FORM{'showsummary'};

# Generate and return the UI (HTML page) from the appropriate template.
print "Content-type: text/html\n\n";
$template->process("bug/dependency-graph.html.tmpl", $vars)
  || ThrowTemplateError($template->error());
