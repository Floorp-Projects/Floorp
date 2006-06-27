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
use Litmus::Cache;
use Litmus::Error;
use Litmus::FormWidget;
use Litmus::Utils;

use CGI;
use Date::Manip;
use JSON;

my $c = Litmus->cgi(); 

# for the moment, you must be an admin to enter tests:
Litmus::Auth::requireAdmin('manage_testgroups.cgi');

my $vars;

my $testgroup_id;
my $message;
my $status;
my $rv;
if ($c->param("testgroup_id")) {
  $testgroup_id = $c->param("testgroup_id");
}
my $rebuild_cache = 0;
my $defaults;
if ($c->param("delete_testgroup_button")) {
  my $testgroup = Litmus::DB::Testgroup->retrieve($testgroup_id);
  if ($testgroup) {
    $rv = $testgroup->delete_with_refs();
    if ($rv) {
      $status = "success";
      $message = "Testgroup ID# $testgroup_id deleted successfully.";
      $rebuild_cache = 1;
    } else {
      $status = "failure";
      $message = "Failed to delete Testgroup ID# $testgroup_id.";
    }
  } else { 
    $status = "failure";
    $message = "Testgroup ID# $testgroup_id does not exist. (Already deleted?)";
  }
} elsif ($c->param("clone_testgroup_button")) {
  my $testgroup = Litmus::DB::Testgroup->retrieve($testgroup_id);
  my $new_testgroup = $testgroup->clone;
  if ($new_testgroup) {
    $status = "success";
    $message = "Testgroup cloned successfully. New testgroup ID# is " . $new_testgroup->testgroup_id;
    $defaults->{'testgroup_id'} = $new_testgroup->testgroup_id;
    $rebuild_cache = 1;
  } else {
    $status = "failure";
    $message = "Failed to clone Testgroup ID# $testgroup_id.";
  }
} elsif ($c->param("editform_mode")) {
  requireField('product', $c->param('product'));
  my $enabled = $c->param('editform_enabled') ? 1 : 0;
  if ($c->param("editform_mode") eq "add") {
    my %hash = (
                name => $c->param('editform_name'),
                product_id => $c->param('product'),
                enabled => $enabled,
               );
    my $new_testgroup = 
      Litmus::DB::Testgroup->create(\%hash);

    if ($new_testgroup) {
      my @selected_subgroups = $c->param("editform_testgroup_subgroups");
      $new_testgroup->update_subgroups(\@selected_subgroups);
      # XXX: Placeholder for updating test runs
      $status = "success";
      $message = "Testgroup added successfully. New testgroup ID# is " . $new_testgroup->testgroup_id;
      $defaults->{'testgroup_id'} = $new_testgroup->testgroup_id;
      $rebuild_cache = 1;
    } else {
      $status = "failure";
      $message = "Failed to add testgroup.";
    }
  } elsif ($c->param("editform_mode") eq "edit") {
    requireField('testgroup_id', $c->param("editform_testgroup_id"));
    $testgroup_id = $c->param("editform_testgroup_id");
    my $testgroup = Litmus::DB::Testgroup->retrieve($testgroup_id);
    if ($testgroup) {
      $testgroup->product_id($c->param('product'));
      $testgroup->enabled($enabled);
      $rv = $testgroup->update();
      if ($rv) {
        my @selected_subgroups = $c->param("editform_testgroup_subgroups");
        $testgroup->update_subgroups(\@selected_subgroups);
        # XXX: Placeholder for updating test runs
        $status = "success";
	$message = "Testgroup ID# $testgroup_id updated successfully.";
        $defaults->{'testgroup_id'} = $testgroup_id;
        $rebuild_cache = 1;
      } else {
	$status = "failure";
	$message = "Failed to update testgroup ID# $testgroup_id.";
      }
    } else {
      $status = "failure";
      $message = "Testgroup ID# $testgroup_id not found.";
    }
  } 
} else {
  $defaults->{'testgroup_id'} = $c->param("testgroup_id");
}

if ($defaults) {
  $vars->{'defaults'} = $defaults;
}

if ($status and $message) {
  $vars->{'onload'} = "toggleMessage('$status','$message');";
}

if ($rebuild_cache) {
  rebuildCache();
}

my $testgroups = Litmus::FormWidget->getTestgroups;
my $products = Litmus::FormWidget->getProducts();
my $subgroups = Litmus::FormWidget->getSubgroups;

my $json = JSON->new(skipinvalid => 1, convblessed => 1);
my $subgroups_js = $json->objToJson($subgroups);

$vars->{'title'} = "Manage Testgroups";
$vars->{'testgroups'} = $testgroups;
$vars->{'products'} = $products;
$vars->{'all_subgroups'} = $subgroups_js;
$vars->{'user'} = Litmus::Auth::getCurrentUser();

my $cookie =  Litmus::Auth::getCookie();
$vars->{"defaultemail"} = $cookie;
$vars->{"show_admin"} = Litmus::Auth::istrusted($cookie);

print $c->header();

Litmus->template()->process("admin/manage_testgroups.tmpl", $vars) || 
  internalError("Error loading template.");

