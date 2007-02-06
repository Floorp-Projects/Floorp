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
# Portions created by Greg Hendricks are Copyright (C) 2007
# Novell. All Rights Reserved.
#
# Contributor(s): Greg Hendricks <ghendricks@novell.com>

use strict;
use lib ".";

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Util;
use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::Product;

use vars qw($vars);
use Data::Dumper;

require 'globals.pl';

my $template = Bugzilla->template;
my $cgi = Bugzilla->cgi;
Bugzilla->login();
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


#print Dumper($plan->access_list);

if ($action eq 'Apply Changes'){
    do_update();
    display();
}
elsif ($action eq 'Add User'){
    do_update();

    my $dbh = Bugzilla->dbh;
    my $userid = DBNameToIdAndCheck(trim($cgi->param('adduser')));
    ThrowUserError('testopia-tester-already-on-list', {'login' => $cgi->param('adduser')}) 
        if ($plan->check_tester($userid));
        
    my $perms = 0;
    
    # The order we check these is important since each permission 
    # implies the prior ones.
    $perms = $cgi->param("nr") ? 1 : $perms;
    $perms = $cgi->param("nw") ? 3 : $perms;
    $perms = $cgi->param("nd") ? 7 : $perms;
    $perms = $cgi->param("na") ? 15 : $perms;

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
    # We need at least on admin    
    my $params = join(" ", $cgi->param());
    ThrowUserErorr('testopia-no-admins') unless $params =~ /a\d+/;
    
    my $tester_regexp = $cgi->param('userregexp');
    trick_taint($tester_regexp);
    
    my $regexp_perms = 0;

    # The order we check these is important since each permission 
    # implies the prior ones.
    $regexp_perms = $cgi->param('pr') ? 1  : $regexp_perms;
    $regexp_perms = $cgi->param('pw') ? 3  : $regexp_perms;
    $regexp_perms = $cgi->param('pd') ? 7  : $regexp_perms;
    $regexp_perms = $cgi->param('pa') ? 15 : $regexp_perms;
    
    $plan->set_tester_regexp($tester_regexp, $regexp_perms);
    
    my $dbh = Bugzilla->dbh;
    foreach my $row (@{$plan->access_list}){
        my $perms = 0;

        # The order we check these is important since each permission 
        # implies the prior ones.
        $perms = $cgi->param('r'.$row->{'user'}->id) ? 1  : $perms;
        $perms = $cgi->param('w'.$row->{'user'}->id) ? 3  : $perms;
        $perms = $cgi->param('d'.$row->{'user'}->id) ? 7  : $perms;
        $perms = $cgi->param('a'.$row->{'user'}->id) ? 15 : $perms;
        
        $plan->update_tester($row->{'user'}->id, $perms);
    }
}

sub display {
    $vars->{'plan'} = $plan;
    $vars->{'user'} = Bugzilla->user;
    $template->process("testopia/admin/access-list.html.tmpl", $vars) 
      || ThrowTemplateError($template->error());
    
}