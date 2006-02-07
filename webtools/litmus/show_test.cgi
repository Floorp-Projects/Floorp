#!/usr/bin/perl -w
# -*- Mode: cperl; indent-tabs-mode: nil -*-
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
use Litmus::Utils;
use Litmus::DB::Resultbug;

use CGI;
use Date::Manip;
use Time::Piece::MySQL;

use diagnostics;

my $c = Litmus->cgi(); 

print $c->header();

my $testid = $c->param("id");

my $vars;
my $cookie =  Litmus::Auth::getCookie();
$vars->{"defaultemail"} = $cookie;
$vars->{"show_admin"} = Litmus::Auth::istrusted($cookie);

if (! $testid) {
    Litmus->template()->process("show/enter_id.html.tmpl", $vars) || 
        internalError(Litmus->template()->error());
    exit;
} 

# Process changes to testcases:
# Only users with canedit can edit testcases.
my @changed;
my $update_status = 0;
if ($c->param("editingTestcases") && 
    Litmus::Auth::canEdit(Litmus::Auth::getCookie())) {
  
  # the editingTestcases param contains a comma-separated list of 
  # testids that the user has made changes to (well, has clicked 
  # the edit button for). 
  @changed = split(',' => $c->param("editingTestcases"));
  foreach my $editid (@changed) {
    my $edittest = Litmus::DB::Test->retrieve($editid);
    if (! $edittest) {invalidInputError("Test $editid does not exist")}
    
    $edittest->summary($c->param("summary_edit_$editid"));
    my $product = Litmus::DB::Product->retrieve($c->param("product_$editid"));
    my $group = Litmus::DB::Testgroup->retrieve($c->param("testgroup_$editid"));
    my $subgroup = Litmus::DB::Subgroup->retrieve($c->param("subgroup_$editid"));
    requireField("product", $product);
    requireField("group", $group);
    requireField("subgroup", $subgroup);
    $edittest->product($product);
    $edittest->testgroup($group);
    $edittest->subgroup($subgroup);
    
    $edittest->steps($c->param("steps_edit_$editid"));
    $edittest->expected_results($c->param("results_edit_$editid"));
    
    if ($c->param("communityenabled_$editid")) {
      $edittest->communityenabled(1);
    } else {
      $edittest->communityenabled(0);
    }
    my $r_bug_id = $c->param("regression_bug_id_$editid");
    if ($r_bug_id eq '' or $r_bug_id eq '0') {
      undef $r_bug_id;
    }
    $edittest->regression_bug_id($r_bug_id);
    $edittest->sort_order($c->param("sort_order_$editid"));
    $edittest->author_id(Litmus::Auth::getCurrentUser());
    $edittest->last_updated(&Date::Manip::UnixDate("now","%q"));
    $edittest->version($edittest->version()+1);
    
    my $update_status = $edittest->update();
    
    my $status = "";
    my $message = "";
    if ($update_status) {
      $status = "success";
      $message = "Testcase ID# $editid updated successfully.";
    } else {
      $status = "failure";
      $message = "Unable to update testcase ID# $editid.";        
    }
    $vars->{'onload'} = "toggleMessage('$status','$message');";
  }
} elsif ($c->param("editingTestcases") && 
         ! Litmus::Auth::canEdit(Litmus::Auth::getCookie())) {
  invalidInputError("You do not have permissions to edit testcases. ");
}

my $test = Litmus::DB::Test->retrieve($testid);

if (! $test) {
    invalidInputError("Test $testid is not a valid test id");
}

my @results = Litmus::DB::Result->retrieve_all();

my $showallresults = $c->param("showallresults") || "";

my @where;
push @where, { field => 'test_id', value => $testid };
my @order_by;
push @order_by, { field => 'created', direction => 'DESC' };
my $test_results = Litmus::DB::Testresult->getTestResults(\@where,\@order_by);

$vars->{'test'} = $test;
$vars->{'results'} = \@results;
$vars->{'showallresults'} = $showallresults;
$vars->{'test_results'} = $test_results;

Litmus->template()->process("show/show.html.tmpl", $vars) || 
        internalError(Litmus->template()->error());    
