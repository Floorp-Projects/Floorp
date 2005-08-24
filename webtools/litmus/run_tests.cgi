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
use Litmus::UserAgentDetect;
use Litmus::SysConfig;
use Litmus::Auth;

use CGI;
use Time::Piece::MySQL;

my $c = new CGI; 

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
    my $user;
    
    my $product = Litmus::DB::Product->retrieve($c->param("product"));
    if (! $product) {
        print $c->header();
        invalidInputError("Invalid product ".$c->param("product"));
    }
    
    if ($c->param("continuetesting")) {
        # they have already gotten setup and just want to 
        # pick a new group or subgroup:
        $sysconfig = Litmus::SysConfig->getCookie($product);
        if (!$sysconfig) {page_pickProduct()};
        $user = Litmus::Auth::getCookie();
        
        print $c->header();
    } else {
        $sysconfig = Litmus::SysConfig->processForm($c);
        # get the user id and set a login cookie:
        my $email = $c->param("email");
        if (!$email) {
            print $c->header();
            invalidInputError("You must enter your email address so we can track your results and contact you if we have any questions.");
        }
        $user = Litmus::DB::User->find_or_create(email => $email);
    
        print $c->header(-cookie => [$sysconfig->setCookie(), Litmus::Auth::setCookie($user)]);
    }
    
    # get all groups for the product:
    my @groups = Litmus::DB::Testgroup->search(product => $sysconfig->product());
    
    # all possible subgroups per group:
    my %subgroups; 
    foreach my $curgroup (@groups) {
        my @subgroups = Litmus::DB::Subgroup->search(testgroup => $curgroup);
        $subgroups{$curgroup->testgroupid()} = \@subgroups;
    }
    
    my $defaultgroup = "";
    if ($c->param("defaulttestgroup")) {
        $defaultgroup = Litmus::DB::Testgroup->
            retrieve($c->param("defaulttestgroup"));
    }
    
    my $vars = {
        opsys     => $sysconfig->opsys(),
        groups    => \@groups,
        subgroups => \%subgroups,
        sysconfig => $sysconfig,
        defaultgroup => $defaultgroup,
    };
    
    Litmus->template()->process("runtests/selectgroupsubgroup.html.tmpl", $vars) || 
        internalError(Litmus->template()->error());    
}

# display a page of testcases:
sub page_test {
    # the form has a subgroup radio button set for each possible group, named
    # subgroup_n where n is the group id number. We get the correct
    # subgroup based on whatever group the user selected: 
    my $subgroupid = $c->param("subgroup_".$c->param("group"));
    
    print $c->header();
    
    # get the tests to display:
    my @tests = Litmus::DB::Test->search(subgroup => $subgroupid,
                                         status => Litmus::DB::Status->search(name => "Enabled"));
    
    # get all possible test results:
    my @results = Litmus::DB::Result->retrieve_all();

    my $vars = {
        sysconfig => Litmus::SysConfig->getCookie($tests[0]->product()),
        group     => Litmus::DB::Subgroup->retrieve($subgroupid)->testgroup(),
        subgroup  => Litmus::DB::Subgroup->retrieve($subgroupid),
        tests       => \@tests,
        results      => \@results,
        istrusted => Litmus::Auth::istrusted(Litmus::Auth::getCookie()),
    };
    
    Litmus->template()->process("runtests/testdisplay.html.tmpl", $vars) ||
        internalError(Litmus->template()->error());
}
