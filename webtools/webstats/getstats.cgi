#!/usr/bonsaitools/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
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
# The Original Code is the Web Page Statistics Generator.
# 
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.
# 
# Contributor(s): Terry Weissman <terry@mozilla.org>

use diagnostics;
use strict;

use Mysql;
use Date::Format;
use Date::Parse;
require 'utils.pl';

use CGI::Carp qw(fatalsToBrowser);
use CGI qw(:standard :html3);

use Chart::Lines;

sub Punt {
    my ($header) = @_;
    print h1($header);
    print p("Please hit " . b("back") . " and try again.");
    exit;
}



print header();

if (!param()) {
    print h1("Web page statistics");

    print start_form("Get");
    print table(Tr(th({-align=>"right"}, "Web pages:"),
                   td(textarea(-name=>"pages",
                               -rows=>8,
                               -columns=>40)),
                   th({-align=>"right"}, "Each line is:"),
                   td(radio_group(-name=>"type",
                                  "-values"=>["exact","substr","regexp"],
                                  -labels=>{"exact"=>"Exact match",
                                            "substr"=>"Substring",
                                            "regexp"=>"Regular expression"}))),
                Tr(th({-align=>"right"}, "Days:"),
                   td(textfield(-name=>"days",
                                -default=>30,
                                -size=>5,
                                -override=>1))));
    print submit();
    print end_form();
    exit();
}

import_names("F");              # Makes all form values available as F::.


# $F::debug = 1;

$::db = Mysql->Connect("localhost", "webstats")
    || die "Can't connect to database server";

$::db = $::db;                  # Make -w shut up.


if ($F::days !~ m/^\d+$/) {
    Punt("Invalid number of days specified.");
}


my $enddate = time2str("%Y-%m-%d", time() - 24*60*60); # Yesterday.
    

my $startdate = time2str("%Y-%m-%d", time() - $F::days * 24*60*60);



my @qstr;
foreach my $pattern (split(/\n/, $F::pages)) {
    my $p = trim($pattern);
    if ($p eq "") {
        next;
    }
    $p = SqlQuote($p);
    if ($F::type eq "exact") {
        push(@qstr, "names.name = $p");
    } elsif ($F::type eq "substr") {
        push(@qstr, "instr(names.name, $p)");
    } elsif ($F::type eq "regexp") {
        push(@qstr, "names.name regexp $p");
    }
}

if (0 == @qstr) {
    Punt("No pages were specified.");
}

my $namepart = join(" or ", @qstr);
my $query = Query ("select count(*) from names where $namepart");
my @row = $query->fetchrow();
if ($row[0] > 10) {
    Punt("Sorry; you selected $row[0] pages, but I can only graph up to 10.");
}




$query = Query("select names.name, stats.date, stats.count from stats, names where stats.id = names.id and stats.date >= '$startdate' and stats.date <= '$enddate' and ($namepart) order by stats.date, names.name");

my %counts;
my %names;
while (my @row = $query->fetchrow()) {
    my ($name, $date, $count) = (@row);

#    print "$name  $date  $count  <BR>\n";

    if (!exists $counts{$date}) {
        $counts{$date} = {};
    }
    $counts{$date}->{$name} = $count;
    $names{$name} = 1;
}

# Go shuffle the data around in the way that the charting package wants.

my @datelist = sort(keys %counts);
my @namelist = sort(keys %names);

my @data;

my @labels = @datelist;
grep(s/^\d+-//, @labels); # Strip years off of labels.

push @data, \@labels;

foreach my $n (@namelist) {
    my $c = [];
    foreach my $d (@datelist) {
        if (exists $counts{$d}->{$n}) {
            push @$c, $counts{$d}->{$n};
        } else {
            push @$c, 0;
        }
    }
    push @data, $c;
}

my $WIDTH = 800;
my $HEIGHT = 600;

my $img = Chart::Lines->new($WIDTH, $HEIGHT);
my $MAXTICKS = 20;      # Try not to show any more x ticks than this.
my $skip = 1;
if (@datelist > $MAXTICKS) {
    $skip = int((@datelist + $MAXTICKS - 1) / $MAXTICKS);
}

my %settings = (
                "title" => "Web hits statistics",
                "x_label" => "Dates",
                "y_label" => "Hits",
                "legend_labels" => \@namelist,
                "skip_x_ticks" => $skip,
                );


$img->set(%settings);

mkdir "data", 0777;

my $imagename = "data/img$$.gif";
open IMAGE, ">$imagename" || die "Can't open $imagename";
$img->gif(*IMAGE, \@data);
close IMAGE;

print "<img src=$imagename width=$WIDTH height=$HEIGHT>\n";
