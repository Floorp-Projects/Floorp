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

# $F::debug = 1;

$::db = Mysql->Connect("localhost", "webstats")
    || die "Can't connect to database server";

$::db = $::db;                  # Make -w shut up.

my $query = Query("select max(date) from stats");
my @row = $query->fetchrow();
my $maxdate = $row[0];
if (!defined $maxdate) {
    $maxdate = 0;
}

my $lastdate = "";

my %ids;
my %counts;


sub Flush {
    my $timeval = str2time($lastdate);
    my $date = time2str("%Y-%m-%d", $timeval);
    my $tick = 0;
    foreach my $name (keys %counts) {
        if ($tick++ % 100 == 0) {
            if ($tick <= 1) {
                print "\nAdding $date ";
            }
            print "+";
        }
        my $id = $ids{$name};
        if (!defined $id) {
            my $q = SqlQuote($name);
            my $query = Query("select id from names where name = $q");
            my @row = $query->fetchrow();
            $id = $row[0];
            if (!defined $id) {
                Query("insert into names (name) values ($q)");
                $query = Query("select LAST_INSERT_ID()");
                @row = $query->fetchrow();
                $id = $row[0];
            }
            $ids{$name} = $id;
        }
        my $count = $counts{$name};
        my $query =
            Query("select count from stats where id = $id and date = '$date'");
        my @row = $query->fetchrow();
        if (defined @row && defined $row[0]) {
            $count += $row[0];
            Query("update stats set count=$count where id = $id and date = '$date'");
        } else {
            Query("insert into stats (id, date, count) values ($id, '$date', $count)");
        }
    }
    undef %counts;
}
    
    


my $tick = 0;
while (<STDIN>) {
    if ($tick++ % 100 == 0) {
        print ".";
    }

    chomp;
    if (! m@\[(\d+)/(\w+)/(\d+):(\d+):(\d+):(\d+).*\].*GET (/\S*) HTTP@) {
	next;
    }
	
    my ($day,$month,$year,$hours,$mins,$secs,$name) =
	($1, $2, $3, $4, $5, $6, $7);
    if (length($name) > 250) {
        $name = substr($name, 0, 250);
    }

    my $date = "$month $day, $year";

    if ($date ne $lastdate) {
        Flush();
        $lastdate = $date;
    }
    $counts{$name}++;
}

Flush();




