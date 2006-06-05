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
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Chris Cooper <ccooper@deadsquid.com>
#   Zach Lipton <zach@zachlipton.com>
#
# ***** END LICENSE BLOCK *****

use strict;

use Time::HiRes qw( gettimeofday tv_interval );
my $t0 = [gettimeofday];

use Litmus;
use Litmus::Error;
use Litmus::DB::Product;
use Litmus::UserAgentDetect;
use Litmus::SysConfig;
use Litmus::Auth;

use CGI;
use Time::Piece::MySQL;

my $title = "Run Tests";

my $c = Litmus->cgi(); 

if ($c->param("group")) { # display the test screen
    page_test();
} elsif ($c->param("platform") || $c->param("continuetesting")) { # pick a group
    page_pickGroupSubgroup();
} else { # need to setup their system config
    page_sysSetup();
}  

# a menu for the user to enter their platform information
sub page_sysSetup {
    print $c->header();
    
    # sometimes the user will have already selected their product
    my $productid = $c->param("product");
    my $product = undef;
    if ($productid) {
        $product = Litmus::DB::Product->retrieve($productid);
        unless ($product) {
            invalidInputError("Invalid product selection: $productid");
        }
    }
    
    Litmus::SysConfig->displayForm($product, "run_tests.cgi");
    exit;
}

# the user has selected their system information and now needs to pick 
# an area to test:
sub page_pickGroupSubgroup {
    my $sysconfig;

    my $product = Litmus::DB::Product->retrieve($c->param("product"));
    if (! $product) {
      print $c->header();
      invalidInputError("Invalid product ".$c->param("product"));
    }

    my $branch;
    if ($c->param("continuetesting")) {
        # they have already gotten setup and just want to 
        # pick a new group or subgroup:
        $sysconfig = Litmus::SysConfig->getCookie($product);
        if (!$sysconfig) {
          page_pickProduct()
        };
        print $c->header();
        $branch = $sysconfig->branch();
    } else {
        $branch = Litmus::DB::Branch->retrieve($c->param("branch"));
        if (! $branch) {
          print $c->header();
          invalidInputError("Invalid branch ".$c->param("branch"));
        }
        $sysconfig = Litmus::SysConfig->processForm($c);
        # get the user id and set a sysconfig cookie
        $c->storeCookie($sysconfig->setCookie());
        print $c->header();
    }
    
    Litmus::Auth::requireLogin("run_tests.cgi");
    
    # Get all groups for the current branch.
    my @groups = Litmus::DB::Testgroup->search_EnabledByBranch($branch->branch_id());

    # all possible subgroups per group:
    my %subgroups; 
    foreach my $curgroup (@groups) {
        my @subgroups = Litmus::DB::Subgroup->search_EnabledByTestgroup($curgroup->testgroup_id());
        $subgroups{$curgroup->testgroup_id()} = \@subgroups;
    }
    
    my $defaultgroup = "";
    if ($c->param("defaulttestgroup")) {
        $defaultgroup = Litmus::DB::Testgroup->
            retrieve($c->param("defaulttestgroup"));
    }

    my $vars = {
        title        => $title,
        user         => Litmus::Auth::getCurrentUser(),
        opsys        => $sysconfig->opsys(),
        groups       => \@groups,
        subgroups    => \%subgroups,
        sysconfig    => $sysconfig,
        defaultgroup => $defaultgroup,
    };

    my $cookie =  Litmus::Auth::getCookie();
    $vars->{"defaultemail"} = $cookie;
    $vars->{"show_admin"} = Litmus::Auth::istrusted($cookie);
    
    Litmus->template()->process("runtests/selectgroupsubgroup.html.tmpl", 
                                $vars) || 
                                  internalError(Litmus->template()->error());

    if ($Litmus::Config::DEBUG) {
      my $elapsed = tv_interval ( $t0 );
      printf  "<div id='pageload'>Page took %f seconds to load.</div>", $elapsed;
    }
}

# display a page of testcases:
sub page_test {
    # the form has a subgroup radio button set for each possible group, named
    # subgroup_n where n is the group id number. We get the correct
    # subgroup based on whatever group the user selected: 
    my $testgroup_id = $c->param("group");
    my $subgroup_id = $c->param("subgroup_".$testgroup_id);
    
    print $c->header();
    
    # get the tests to display:
    my @tests = Litmus::DB::Testcase->search_CommunityEnabledBySubgroup($subgroup_id);
    
    # get all possible test results:
    # my @results = Litmus::DB::Result->retrieve_all();

    my $sysconfig = Litmus::SysConfig->getCookie($tests[0]->product());
    
    my $cookie =  Litmus::Auth::getCookie();

    my $vars = {
                title     => $title,
                sysconfig => Litmus::SysConfig->getCookie($tests[0]->product()),
                group     => Litmus::DB::Testgroup->retrieve($testgroup_id),
                subgroup  => Litmus::DB::Subgroup->retrieve($subgroup_id),
                tests     => \@tests,
#                results   => \@results,
                defaultemail => $cookie,
                show_admin   => Litmus::Auth::istrusted($cookie),
                istrusted    => Litmus::Auth::istrusted($cookie),
    };

    $vars->{"defaultemail"} = $cookie;
    $vars->{"show_admin"} = Litmus::Auth::istrusted($cookie);
    $vars->{"istrusted"} = Litmus::Auth::istrusted($cookie);
    
    Litmus->template()->process("runtests/testdisplay.html.tmpl", $vars) ||
        internalError(Litmus->template()->error());
    
    if ($Litmus::Config::DEBUG) {
      my $elapsed = tv_interval ( $t0 );
      printf  "<div id='pageload'>Page took %f seconds to load.</div>", $elapsed;
    }
}
