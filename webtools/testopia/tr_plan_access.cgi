#!/usr/bin/perl -wT
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
# The Original Code is the Bugzilla Testopia System.
#
# The Initial Developer of the Original Code is Greg Hendricks.
# Portions created by Greg Hendricks are Copyright (C) 2006
# Novell. All Rights Reserved.
#
# Contributor(s): Greg Hendricks <ghendricks@novell.com>

use strict;
use lib ".";

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Testopia::Constants;
use Bugzilla::Error;
use Bugzilla::Util;
use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::Product;

use vars qw($vars);

require 'globals.pl';

my $template = Bugzilla->template;
my $cgi = Bugzilla->cgi;

Bugzilla->login(LOGIN_REQUIRED);
print $cgi->header;

my $plan_id = trim($cgi->param('plan_id') || '');
my $action = $cgi->param('action') || '';

unless (detaint_natural($plan_id)){
  $vars->{'form_action'} = 'tr_plan_access.cgi';
  $template->process("testopia/plan/choose.html.tmpl", $vars) 
      || ThrowTemplateError($template->error());
  exit;
}

my $plan = Bugzilla::Testopia::TestPlan->new($plan_id);

unless ($plan->canadmin){
     ThrowUserError('testopia-plan-acl-denied', {plan_id => $plan->id});
}

if ($action eq 'Apply Changes'){
    do_update();
    display();
}
elsif ($action eq 'Add User'){
    do_update();

    my $userid = Bugzilla->user->login_to_id(trim($cgi->param('adduser'))) || ThrowUserError("invalid_username", { name => $cgi->param('adduser')});
    ThrowUserError('testopia-tester-already-on-list', {'login' => $cgi->param('adduser')}) 
        if ($plan->check_tester($userid));
        
    my $perms = 0;
    
    $perms |= TR_READ   if $cgi->param("nr");
    $perms |= TR_READ | TR_WRITE  if $cgi->param("nw");
    $perms |= TR_READ | TR_WRITE | TR_DELETE if $cgi->param("nd");
    $perms |= TR_READ | TR_WRITE | TR_DELETE | TR_ADMIN  if $cgi->param("na");
    
    detaint_natural($perms);
    $plan->add_tester($userid, $perms); 

    display();
}
elsif ($action eq 'delete'){
    my $userid = $cgi->param('user');
    detaint_natural($userid);
    my $user = Bugzilla::User->new($userid);
    $plan->remove_tester($user->id);
    $vars->{'tr_message'} = $user->login ." Removed from Plan";
    display();
}
    
else{
    display();
}

sub do_update {
    # We need at least one admin    
    my $params = join(" ", $cgi->param());
    ThrowUserError('testopia-no-admins') unless $params =~ /(^|\s)a\d+($|\s)/;
    
    my $tester_regexp = $cgi->param('userregexp');
    trick_taint($tester_regexp);
    
    my $regexp_perms = 0;

    # Each permission implies the prior ones.
    $regexp_perms |= TR_READ   if $cgi->param('pr');
    $regexp_perms |= TR_READ | TR_WRITE  if $cgi->param('pw');
    $regexp_perms |= TR_READ | TR_WRITE | TR_DELETE if $cgi->param('pd');
    $regexp_perms |= TR_READ | TR_WRITE | TR_DELETE | TR_ADMIN  if $cgi->param('pa');
    
    detaint_natural($regexp_perms);
    $plan->set_tester_regexp($tester_regexp, $regexp_perms);
    
    foreach my $row (@{$plan->access_list}){
        my $perms = 0;

        $perms |= TR_READ   if $cgi->param('r'.$row->{'user'}->id);
        $perms |= TR_READ | TR_WRITE  if $cgi->param('w'.$row->{'user'}->id);
        $perms |= TR_READ | TR_WRITE | TR_DELETE if $cgi->param('d'.$row->{'user'}->id);
        $perms |= TR_READ | TR_WRITE | TR_DELETE | TR_ADMIN  if $cgi->param('a'.$row->{'user'}->id);
        
        detaint_natural($perms);
        $plan->update_tester($row->{'user'}->id, $perms);
    }
}

sub display {
    $vars->{'plan'} = $plan;
    $vars->{'user'} = Bugzilla->user;
    $template->process("testopia/admin/access-list.html.tmpl", $vars) 
      || ThrowTemplateError($template->error());
    
}