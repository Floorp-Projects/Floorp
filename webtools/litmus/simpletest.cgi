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
use Litmus::Error;
use Litmus::DB::Test;
use Litmus::UserAgentDetect;
use Litmus::SysConfig;
use Litmus::Auth;

use CGI;
use Time::Piece::MySQL;

my $c = new CGI; 

# how old of a build do we want to allow? default is 10 days
my $maxbuildage = 10; 
# what branch do we accept? default is the trunk or the 1.8 branch
my $branch = Litmus::DB::Branch->retrieve(1);
my $branch2 = Litmus::DB::Branch->retrieve(2);

showTest();

sub showTest {
    print $c->header();
    
    # if they don't have a buildid or it's old, then just don't display 
    # any tests:
    my $ua = Litmus::UserAgentDetect->new();
    my $time = localtime;
    $time->date_separator("");
    my $curbuildtime = $time->ymd;
    my $prod = Litmus::DB::Product->search(name => "Firefox")->next();
    my @detectbranch = $ua->branch($prod);
    if ((! $ua->buildid()) || (! $detectbranch[0]) || 
        $curbuildtime - $ua->buildid() > $maxbuildage ||
        ($detectbranch[0]->branchid() != $branch->branchid() &&
          $detectbranch[0]->branchid() != $branch2->branchid())) {
            Litmus->template()->process("simpletest/simpletest.html.tmpl") || 
              internalError(Litmus->template()->error());
           exit;
    }
    
    my $pid = $prod->productid();
    # get a random test to display:
    Litmus::DB::Test->set_sql(random_test => qq {
        SELECT tests.test_id, tests.subgroup_id, tests.summary, tests.details, tests.status_id, tests.community_enabled, tests.format_id, tests.regression_bug_id
        FROM __TABLE__, products,test_groups,subgroups
        WHERE
            products.product_id=? AND 
            community_enabled = 1 AND
            products.product_id=test_groups.product_id AND 
            subgroups.testgroup_id=test_groups.testgroup_id AND 
            tests.subgroup_id=subgroups.subgroup_id 
        ORDER BY RAND()    
        LIMIT 1
    });

    my $test = Litmus::DB::Test->search_random_test($pid)->next();
    
    my @results = Litmus::DB::Result->retrieve_all();
    
    my $vars = {
        test => $test,
        results => \@results,
    };

    $vars->{"defaultemail"} = Litmus::Auth::getCookie();
    
    Litmus->template()->process("simpletest/simpletest.html.tmpl", $vars) || 
        internalError(Litmus->template()->error());
}
