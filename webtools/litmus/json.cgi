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

use Litmus;
use Litmus::Auth;
use Litmus::Error;
use Text::Markdown;
use JSON;

use CGI;
use Date::Manip;

my $c = Litmus->cgi(); 
print $c->header('text/plain');

if ($c->param("testcase_id")) {
  my $testcase_id = $c->param("testcase_id");
  my $testcase = Litmus::DB::Testcase->retrieve($testcase_id);
  my @testgroups = Litmus::DB::Testgroup->search_EnabledByTestcase($testcase_id);
  my @subgroups = Litmus::DB::Subgroup->search_EnabledByTestcase($testcase_id);
  $testcase->{'testgroups'} = \@testgroups;
  $testcase->{'subgroups'} = \@subgroups;
  my $json = JSON->new(skipinvalid => 1, convblessed => 1);
  
  # apply markdown formatting to the steps and expected results:
  $testcase->{'steps_formatted'} = Text::Markdown::markdown($testcase->steps());
  $testcase->{'expected_results_formatted'} = Text::Markdown::markdown($testcase->expected_results());
  
  my $js = $json->objToJson($testcase);
  print $js;
} elsif ($c->param("subgroup_id")) {
  my $subgroup_id = $c->param("subgroup_id");
  my $subgroup = Litmus::DB::Subgroup->retrieve($subgroup_id);
  my @testgroups = Litmus::DB::Testgroup->search_EnabledBySubgroup($subgroup_id);
  my @testcases = Litmus::DB::Testcase->search_EnabledBySubgroup($subgroup_id);
  $subgroup->{'testgroups'} = \@testgroups;
  $subgroup->{'testcases'} = \@testcases;
  my $json = JSON->new(skipinvalid => 1, convblessed => 1);
  my $js = $json->objToJson($subgroup);
  print $js;
} elsif ($c->param("testgroup_id")) {
  my $testgroup_id = $c->param("testgroup_id");
  my $testgroup = Litmus::DB::Testgroup->retrieve($testgroup_id);
  my @subgroups = Litmus::DB::Subgroup->search_EnabledByTestgroup($testgroup_id);
  $testgroup->{'subgroups'} = \@subgroups;
  my $json = JSON->new(skipinvalid => 1, convblessed => 1);
  my $js = $json->objToJson($testgroup);
  print $js;
} elsif ($c->param("product_id")) {
  my $product_id = $c->param("product_id");
  my $product = Litmus::DB::Product->retrieve($product_id);
  my $json = JSON->new(skipinvalid => 1, convblessed => 1);
  my $js = $json->objToJson($product);
  print $js;
} elsif ($c->param("platform_id")) {
  my $platform_id = $c->param("platform_id");
  my $platform = Litmus::DB::Platform->retrieve($platform_id);
  my @products = Litmus::DB::Product->search_ByPlatform($platform_id);
  $platform->{'products'} = \@products;
  my $json = JSON->new(skipinvalid => 1, convblessed => 1);
  my $js = $json->objToJson($platform);
  print $js;
} elsif ($c->param("opsys_id")) {
  my $opsys_id = $c->param("opsys_id");
  my $opsys = Litmus::DB::Opsys->retrieve($opsys_id);
  my $json = JSON->new(skipinvalid => 1, convblessed => 1);
  my $js = $json->objToJson($opsys);
  print $js;
} elsif ($c->param("branch_id")) {
  my $branch_id = $c->param("branch_id");
  my $branch = Litmus::DB::Branch->retrieve($branch_id);
  my $json = JSON->new(skipinvalid => 1, convblessed => 1);
  my $js = $json->objToJson($branch);
  print $js;
}



