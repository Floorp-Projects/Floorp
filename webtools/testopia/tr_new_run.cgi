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
# The Original Code is the Bugzilla Test Runner System.
#
# The Initial Developer of the Original Code is Maciej Maczynski.
# Portions created by Maciej Maczynski are Copyright (C) 2001
# Maciej Maczynski. All Rights Reserved.
#
# Contributor(s): Greg Hendricks <ghendricks@novell.com>

use strict;
use lib ".";

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Util;

use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::TestRun;
use Bugzilla::Testopia::Search;
use Bugzilla::Testopia::Table;

require "globals.pl";

use vars qw($template $vars);

Bugzilla->login(LOGIN_REQUIRED);
ThrowUserError("testopia-create-denied", {'object' => 'Test Run'}) unless UserInGroup('edittestcases');

print Bugzilla->cgi->header();
   
my $dbh = Bugzilla->dbh;
my $cgi = Bugzilla->cgi;

push @{$::vars->{'style_urls'}}, 'testopia/css/default.css';

my $action = $cgi->param('action') || '';
my $plan_id = $cgi->param('plan_id');
detaint_natural($plan_id);
validate_test_id($plan_id, 'plan');

ThrowUserError("testopia-missing-parameter", {'param' => 'plan_id'}) unless ($plan_id);
my $plan = Bugzilla::Testopia::TestPlan->new($plan_id);
ThrowUserError('testopia-create-build', {'plan' => $plan}) unless (scalar @{$plan->builds} >0);

if ($action eq 'Add'){
    my $manager  = DBNameToIdAndCheck($cgi->param('manager'));
    my $status   = $cgi->param('status');
    my $prodver  = $cgi->param('product_version');
    my $pversion = $cgi->param('plan_version');
    my $build    = $cgi->param('build');
    my $summary  = $cgi->param('summary');
    my $notes    = $cgi->param('notes');    
    my $env      = $cgi->param('environment');

    ThrowUserError('testopia-missing-required-field', {'field' => 'summary'}) if $summary  eq '';
    
    detaint_natural($status);
    detaint_natural($build);
    detaint_natural($pversion);
    detaint_natural($env);
        
    # All inserts are done with placeholders so this is OK
    trick_taint($summary);
    trick_taint($notes);
    trick_taint($prodver);
    
    my $reg = qr/c_([\d]+)/;
    my @c;
    foreach my $p ($cgi->param()){
        push @c, $1 if $p =~ $reg;
    }
    
    my $run = Bugzilla::Testopia::TestRun->new({
            'plan_id'           => $plan_id,
            'environment_id'    => $env,
            'build_id'          => $build,
            'product_version'   => $prodver,
            'plan_text_version' => $pversion,
            'manager_id'        => $manager,
            'summary'        => $summary,
            'notes'     => $notes,
            'plan'      => $plan,
    });
    my $run_id = $run->store;
    $run = Bugzilla::Testopia::TestRun->new($run_id);
    foreach my $case_id (@c){  
        $run->add_case_run($case_id);
    }
    # clear the params so we don't confuse search.
    $cgi->delete_all;

    $cgi->param('current_tab', 'case_run');
    $cgi->param('run_id', $run_id);
    my $search = Bugzilla::Testopia::Search->new($cgi);
    my $table = Bugzilla::Testopia::Table->new('case_run', 'tr_show_run.cgi', $cgi, undef, $search->query);

    $vars->{'run'} = $run;
    $vars->{'table'} = $table;
    $vars->{'action'} = 'Commit';
    $template->process("testopia/run/show.html.tmpl", $vars) ||
        ThrowTemplateError($template->error());
    
}

####################
### Display Form ###
####################
else {
    $cgi->param('current_tab', 'case');
    $cgi->param('viewall', 1);
    my $search = Bugzilla::Testopia::Search->new($cgi);
    my $table = Bugzilla::Testopia::Table->new('case', 'tr_new_run.cgi', $cgi, undef, $search->query);
    $vars->{'table'} = $table;    
    $vars->{'dotweak'} = 1;
    $vars->{'fullwidth'} = 1; #novellonly
    $vars->{'plan'} = $plan;
    $vars->{'action'} = 'Add';
    my $run = Bugzilla::Testopia::TestRun->new(
                        {'run_id' => 0,
                         'plan'   => $plan,
                         'plan_text_version' => $plan->version } );
    ThrowUserError('testopia-create-environment') unless (scalar @{$run->environments} > 0);
    $vars->{'run'} = $run;
    $template->process("testopia/run/add.html.tmpl", $vars) ||
        ThrowTemplateError($template->error());
}
