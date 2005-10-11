#!/usr/bin/perl -w
# -*- mode: cperl; c-basic-offset: 8; indent-tabs-mode: nil; -*-

# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Litmus.
#
# The Initial Developer of the Original Code is
# the Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Chris Cooper <ccooper@deadsquid.com>
#   Zach Lipton <zach@zachlipton.com>
#
# ***** END LICENSE BLOCK *****

use strict;

use Litmus;
use Litmus::Auth;
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

# find the number of results in the database:
my $numresults = Litmus::DB::Testresult->count_all();

# get a list of the top 15 testers of all time, sorted by the number 
# of test results submitted:
my $dbh = Litmus::DB::User->db_Main();
my $sth = $dbh->prepare("SELECT users.user_id, users.email, count(*) AS thecount
                                         FROM users, test_results 
                                         WHERE 
                                             users.user_id=test_results.user_id 
                                         GROUP BY user_id 
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
    title      => "Statistics",
    numtests   => $numtests,
    numusers   => $numusers,
    numresults => $numresults,
    toptesters => \@toptesters,
};

my $cookie =  Litmus::Auth::getCookie();
$vars->{"defaultemail"} = $cookie;
$vars->{"show_admin"} = Litmus::Auth::istrusted($cookie);

Litmus->template()->process("stats/stats.html.tmpl", $vars) || 
        internalError(Litmus->template()->error());    
        
