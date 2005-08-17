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

# get a list of the top 15 testers of all time, sorted by the number 
# of test results submitted:
Litmus::DB::User->set_sql('toptesters', "SELECT users.userid, count(*) AS count
										 FROM users, testresults 
										 WHERE 
										 	users.userid=testresults.user 
										 GROUP BY user 
										 ORDER BY count DESC 
 										 LIMIT 15;");
my @testers = Litmus::DB::User->search_toptesters();
my @toptesters;
foreach my $curtester (@testers) {
	my %testerinfo;
	$testerinfo{"email"} = $curtester->email();
	$testerinfo{"numtests"} = $curtester->count();
	push(@toptesters, \%testerinfo);
}


my $vars = {
	toptesters => \@toptesters,
};

Litmus->template()->process("stats/stats.html.tmpl", $vars) || 
        internalError(Litmus->template()->error());    
        
