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

# Litmus homepage

use Litmus;
use Litmus::Auth;
use Litmus::Cache;
use Litmus::Error;
use Litmus::FormWidget;
use Litmus::Utils;

use CGI;
use Date::Manip;

my $c = Litmus->cgi(); 

# for the moment, you must be an admin to enter tests:
Litmus::Auth::requireAdmin('manage_testcases.cgi');

my $vars;

my $testcase_id;
my $message;
my $status;
my $rv;
if ($c->param("testcase_id")) {
  $testcase_id = $c->param("testcase_id");
}

my $rebuild_cache = 0;
if ($c->param("delete_testcase_button")) {
  my $testcase = Litmus::DB::Testcase->retrieve($testcase_id);
  if ($testcase) {
    $rv = $testcase->delete_with_refs();
    if ($rv) {
      $status = "success";
      $message = "Testcase ID# $testcase_id deleted successfully.";
      $rebuild_cache=1;
    } else {
      $status = "failure";
      $message = "Failed to delete Testcase ID# $testcase_id.";
    }
  } else { 
    $status = "failure";
    $message = "Testcase ID# $testcase_id does not exist. (Already deleted?)";
  }
} elsif ($c->param("clone_testcase_button")) {
  my $testcase = Litmus::DB::Testcase->retrieve($testcase_id);
  my $new_testcase = $testcase->clone;
  if ($new_testcase) {
    $status = "success";
    $message = "Testcase cloned successfully. New testcase ID# is " . $new_testcase->testcase_id;
    $rebuild_cache=1;
  } else {
    $status = "failure";
    $message = "Failed to clone Testcase ID# $testcase_id.";
  }
} elsif ($c->param("editform_mode")) {
  requireField('summary', $c->param('editform_summary'));
  requireField('product', $c->param('product'));
  requireField('subgroup', $c->param('subgroup'));
  requireField('author', $c->param('editform_author_id'));
  my $enabled = $c->param('editform_enabled') ? 1 : 0;
  my $community_enabled = $c->param('editform_communityenabled') ? 1 : 0;
  my $now = &UnixDate("today","%q");
  if ($c->param("editform_mode") eq "add") {
    my %hash = (
                summary => $c->param('editform_summary'),
                steps => $c->param('editform_steps') ? $c->param('editform_steps') : '',
                expected_results => $c->param('editform_results') ? $c->param('editform_results') : '',
                product_id => $c->param('product'),
                enabled => $enabled,
                community_enabled => $community_enabled,
                regression_bug_id => $c->param('editform_regression_bug_id') ? $c->param('editform_regression_bug_id') : '',
                author_id => $c->param('editform_author_id'),
                creation_date => $now,
                last_updated => $now,
               );
    my $new_testcase = 
      Litmus::DB::Testcase->create(\%hash);

    if ($new_testcase) {      
      my @selected_subgroups = $c->param("subgroup");
      $new_testcase->update_subgroups(\@selected_subgroups);
      $status = "success";
      $message = "Testcase added successfully. New testcase ID# is " . $new_testcase->testcase_id;
      $rebuild_cache=1;
    } else {
      $status = "failure";
      $message = "Failed to add testcase.";        
    }
    
  } elsif ($c->param("editform_mode") eq "edit") {
    requireField('testcase_id', $c->param("editform_testcase_id"));
    $testcase_id = $c->param("editform_testcase_id");
    my $testcase = Litmus::DB::Testcase->retrieve($testcase_id);
    if ($testcase) {
      $testcase->summary($c->param('editform_summary'));
      $testcase->steps($c->param('editform_steps') ? $c->param('editform_steps') : '');
      $testcase->expected_results($c->param('editform_results') ? $c->param('editform_results') : '');
      $testcase->product_id($c->param('product'));
      $testcase->enabled($enabled);
      $testcase->community_enabled($community_enabled);
      $testcase->regression_bug_id($c->param('editform_regression_bug_id') ? $c->param('editform_regression_bug_id') : '');
      $testcase->author_id($c->param('editform_author_id'));
      $testcase->last_updated($now);
      $testcase->version($testcase->version + 1);
      $rv = $testcase->update();
      if ($rv) {
        my @selected_subgroups = $c->param("subgroup");
        $testcase->update_subgroups(\@selected_subgroups);
        $status = "success";
	$message = "Testcase ID# $testcase_id updated successfully.";
        $rebuild_cache=1;
      } else {
	$status = "failure";
	$message = "Failed to update testcase ID# $testcase_id.";        
      }
    } else {
      $status = "failure";
      $message = "Testcase ID# $testcase_id not found.";        
    }
  } 
} else {
  my $defaults;
  $defaults->{'testcase_id'} = $c->param("testcase_id");
  $vars->{'defaults'} = $defaults;  
}

if ($status and $message) {
  $vars->{'onload'} = "toggleMessage('$status','$message');";
}

if ($rebuild_cache) {
  rebuildCache();
}

my $testcases = Litmus::FormWidget->getTestcases;
my $products = Litmus::FormWidget->getProducts();
my $authors = Litmus::FormWidget->getAuthors();

$vars->{'title'} = "Manage Testcases";
$vars->{'testcases'} = $testcases;
$vars->{'products'} = $products;
$vars->{'authors'} = $authors;
$vars->{'user'} = Litmus::Auth::getCurrentUser();

my $cookie =  Litmus::Auth::getCookie();
$vars->{"defaultemail"} = $cookie;
$vars->{"show_admin"} = Litmus::Auth::istrusted($cookie);

print $c->header();

Litmus->template()->process("admin/manage_testcases.tmpl", $vars) || 
  internalError("Error loading template.");

