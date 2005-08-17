#!/usr/bin/perl -w
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
# The Original Code is Litmus.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Zach Lipton <zach@zachlipton.com>

use strict;

use Litmus;
use Litmus::Error;
use Litmus::DB::Product;

use CGI;
use Time::Piece::MySQL;

my $c = new CGI; 

print $c->header();

# find the number of testcases in the database
my $numtests = Litmus::DB::Test->count_all();

# find the number of users in the database
my $numusers = Litmus::DB::User->count_all();

# get a list of the top 15 testers of all time, sorted by the number 
# of test results submitted:
my $dbh = Litmus::DB::User->db_Main();
my $sth = $dbh->prepare("SELECT userid, email, count(*) AS thecount
                                         FROM users, testresults 
                                         WHERE 
                                             users.userid=testresults.user 
                                         GROUP BY user 
                                         ORDER BY thecount DESC 
                                          LIMIT 15;");
$sth->execute();
my @toptesters;
my @curtester;
while (@curtester = $sth->fetchrow_array()) {
    my %testerinfo;
    $testerinfo{"email"} = $curtester[1];
    $testerinfo{"numtests"} = $curtester[2];
    push(@toptesters, \%testerinfo);
}


my $vars = {
    numtests   => $numtests,
    numusers   => $numusers,
    toptesters => \@toptesters,
};

Litmus->template()->process("stats/stats.html.tmpl", $vars) || 
        internalError(Litmus->template()->error());    
        
