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
use Bugzilla::User;

use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::TestRun;
use Bugzilla::Testopia::Search;
use Bugzilla::Testopia::Table;

require 'globals.pl';

use vars qw($vars);
my $template = Bugzilla->template;
my $query_limit = 10000;

Bugzilla->login(LOGIN_REQUIRED);
my $cgi = Bugzilla->cgi;

push @{$::vars->{'style_urls'}}, 'testopia/css/default.css';

my $action = $cgi->param('action') || '';
my $plan_id = $cgi->param('plan_id');

unless ($plan_id){
  $vars->{'form_action'} = 'tr_new_run.cgi';
  print $cgi->header;
  $template->process("testopia/plan/choose.html.tmpl", $vars) 
      || ThrowTemplateError($template->error());
  exit;
}

detaint_natural($plan_id);
validate_test_id($plan_id, 'plan');

my $plan = Bugzilla::Testopia::TestPlan->new($plan_id);

unless ($plan->canedit){
    print $cgi->header;
    ThrowUserError("testopia-create-denied", {'object' => 'Test Run'});
}

unless (scalar @{$plan->product->builds(1)} > 0){
    print $cgi->header;
    ThrowUserError('testopia-create-build', {'plan' => $plan});
}

if ($action eq 'Add'){
    my $serverpush = support_server_push($cgi);
    if ($serverpush) {
        print $cgi->multipart_init;
        print $cgi->multipart_start;
    
        $template->process("list/server-push.html.tmpl", $vars)
          || ThrowTemplateError($template->error());
    }

    my $manager  = login_to_id(trim($cgi->param('manager')));
    my $status   = $cgi->param('status');
    my $prodver  = $cgi->param('product_version');
    my $pversion = $cgi->param('plan_version');
    my $build    = $cgi->param('build');
    my $summary  = $cgi->param('summary');
    my $notes    = $cgi->param('notes');    
    my $env      = $cgi->param('environment') ? $cgi->param('environment') : $cgi->param('env_pick');
    if ($summary  eq ''){
        print $cgi->multipart_end if $serverpush;
        ThrowUserError('testopia-missing-required-field', {'field' => 'summary'});
    }
    if ($env  eq ''){
        print $cgi->multipart_end if $serverpush;
        ThrowUserError('testopia-missing-required-field', {'field' => 'environment'});
    }
    unless ($manager){
       print $cgi->multipart_end if $serverpush;
       ThrowUserError("invalid_username", { name => $cgi->param('assignee') });
    }
    
    validate_test_id($env, 'environment');
     
    detaint_natural($status);
    detaint_natural($build);
    detaint_natural($pversion);
    detaint_natural($env);
        
    # All inserts are done with placeholders so this is OK
    trick_taint($summary);
    trick_taint($notes);
    trick_taint($prodver);
    
    
    if ($cgi->param('new_build')){
        my $new_build   = $cgi->param('new_build');
        trick_taint($new_build);
        my $b = Bugzilla::Testopia::Build->new({
                'name'        => $new_build,
                'milestone'   => '---',
                'product_id'  => $plan->product_id,
                'description' => '',
                'isactive'    => 1, 
        });
        my $bid = $b->check_name($new_build);
        $bid ? $build = $bid : $build = $b->store; 
    }
    
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

    my $progress_interval = 250;
    my $i = 0;
    my $total = scalar @c;
    foreach my $case_id (@c){
        $i++;
        if ($i % $progress_interval == 0 && $serverpush){
            print $cgi->multipart_end;
            print $cgi->multipart_start;
            $vars->{'complete'} = $i;
            $vars->{'total'} = $total;
            $template->process("testopia/progress.html.tmpl", $vars)
              || ThrowTemplateError($template->error());
        }  
        $run->add_case_run($case_id);
    }
    # clear the params so we don't confuse search.
    $cgi->delete_all;
    if ($serverpush) {
        print $cgi->multipart_end;
        print $cgi->multipart_start;
    } else {
        print $cgi->header;
    }
    $cgi->param('current_tab', 'case_run');
    $cgi->param('run_id', $run_id);
    my $search = Bugzilla::Testopia::Search->new($cgi);
    my $table = Bugzilla::Testopia::Table->new('case_run', 'tr_show_run.cgi', $cgi, undef, $search->query);
    if ($table->view_count > $query_limit){
        print $cgi->multipart_end if $serverpush;
        ThrowUserError('testopia-query-too-large', {'limit' => $query_limit});
    }
    
    $vars->{'run'} = $run;
    $vars->{'table'} = $table;
    $vars->{'action'} = 'Commit';
    $vars->{'form_action'} = 'tr_show_run.cgi';
    $vars->{'tr_message'} = "Test Run: \"". $run->summary ."\" created successfully.";
    $vars->{'backlink'} = $run;
    $template->process("testopia/run/show.html.tmpl", $vars) ||
        ThrowTemplateError($template->error());
    print $cgi->multipart_final if $serverpush;
    
}

####################
### Display Form ###
####################
else {
    $cgi->param('current_tab', 'case');
    my $search = Bugzilla::Testopia::Search->new($cgi);
    my $table = Bugzilla::Testopia::Table->new('case', 'tr_new_run.cgi', $cgi, undef, $search->query);
    $vars->{'case'} = Bugzilla::Testopia::TestCase->new({'case_id' => 0});
    $vars->{'table'} = $table;    
    $vars->{'dotweak'} = 1;
    $vars->{'fullwidth'} = 1; #novellonly
    $vars->{'plan'} = $plan;
    $vars->{'action'} = 'Add';
    my $run = Bugzilla::Testopia::TestRun->new(
                        {'run_id' => 0,
                         'plan'   => $plan,
                         'plan_text_version' => $plan->version } );
    print $cgi->header;
    ThrowUserError('testopia-create-environment') unless (scalar @{$run->environments} > 0);
    $vars->{'run'} = $run;
    $template->process("testopia/run/add.html.tmpl", $vars) ||
        ThrowTemplateError($template->error());
}
