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
# Contributor(s):  Greg Hendricks <ghendricks@novell.com>

use strict;
use lib ".";

use Bugzilla;
use Bugzilla::Config;
use Bugzilla::Error;
use Bugzilla::Constants;
use Bugzilla::Util;
use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::Search;
use Bugzilla::Testopia::Table;
use Bugzilla::Testopia::TestRun;

use vars qw($vars $template);

require "globals.pl";
my $dbh = Bugzilla->dbh;
my $cgi = Bugzilla->cgi;
my $template = Bugzilla->template;
my $query_limit = 5000;

Bugzilla->login();
my $serverpush = support_server_push($cgi);
if ($serverpush) {
    print $cgi->multipart_init;
    print $cgi->multipart_start;

    $template->process("list/server-push.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}
else {
    print $cgi->header;
}
# prevent DOS attacks from multiple refreshes of large data
$::SIG{TERM} = 'DEFAULT';
$::SIG{PIPE} = 'DEFAULT';

my $action = $cgi->param('action') || '';
if ($action eq 'Commit'){
    Bugzilla->login(LOGIN_REQUIRED);
    # Get the list of checked items. This way we don't have to cycle through 
    # every test case, only the ones that are checked.
    my $reg = qr/r_([\d]+)/;
    my $params = join(" ", $cgi->param());
    my @params = $cgi->param();
    unless ($params =~ $reg){
        print $cgi->multipart_end if $serverpush;
        ThrowUserError('testopia-none-selected', {'object' => 'run'});
    }
    if ($cgi->param('environment') eq ''){
        print $cgi->multipart_end if $serverpush;
        ThrowUserError('testopia-missing-required-field', {'field' => 'environment'});
    }
    
    my $progress_interval = 250;
    my $i = 0;
    my $total = scalar @params;

    foreach my $p ($cgi->param()){
        my $run = Bugzilla::Testopia::TestRun->new($1) if $p =~ $reg;
        next unless $run;
        
        $i++;
        if ($i % $progress_interval == 0 && $serverpush){
            print $cgi->multipart_end;
            print $cgi->multipart_start;
            $vars->{'complete'} = $i;
            $vars->{'total'} = $total;
            $template->process("testopia/progress.html.tmpl", $vars)
              || ThrowTemplateError($template->error());
        }
        unless ($run->canedit){
            print $cgi->multipart_end if $serverpush;
            ThrowUserError("testopia-read-only", {'object' => 'run', 'id' => $run->id});
        }
        my $manager   = DBNameToIdAndCheck(trim($cgi->param('manager'))) if $cgi->param('manager');
        my $status;
        if ($cgi->param('run_status')){
            if ($cgi->param('run_status') == -1){
                $status = $run->stop_date;
            }
            else {
                $status = get_time_stamp();
            }
        }
        
        my $enviro    = $cgi->param('environment')   eq '--Do Not Change--' ? $run->environment->id : $cgi->param('environment');
        my $build     = $cgi->param('build') == -1 ? $run->build->id : $cgi->param('build');

        detaint_natural($status);
        validate_test_id($enviro, 'environment');
        validate_test_id($build, 'build');
        my %newvalues = ( 
            'manager_id'        => $manager || $run->manager->id,
            'stop_date'         => $status,
            'environment_id'    => $enviro,
            'build_id'          => $build
        );
        $run->update(\%newvalues);
        if ($cgi->param('addtags')){
            foreach my $name (split(/[,]+/, $cgi->param('addtags'))){
                trick_taint($name);
                my $tag = Bugzilla::Testopia::TestTag->new({'tag_name' => $name});
                my $tag_id = $tag->store;
                $run->add_tag($tag_id);
            }
        }     
    }
    if ($serverpush && !$cgi->param('debug')) {
        print $cgi->multipart_end;
        print $cgi->multipart_start;
    }
    my $run = Bugzilla::Testopia::TestRun->new({ 'run_id' => 0 });
    $vars->{'run'} = $run;
    $vars->{'title'} = "Update Successful";
    $vars->{'tr_message'} = "$i Test Runs Updated";
    $vars->{'current_tab'} = 'run';
    $vars->{'build_list'} = $run->get_distinct_builds();
    $template->process("testopia/search/advanced.html.tmpl", $vars)
        || ThrowTemplateError($template->error()); 
    print $cgi->multipart_final if $serverpush;
    exit;
    
}
else {
    $vars->{'qname'} = $cgi->param('qname') if $cgi->param('qname');
    $cgi->param('current_tab', 'run');
    my $search = Bugzilla::Testopia::Search->new($cgi);
    my $table = Bugzilla::Testopia::Table->new('run', 'tr_list_runs.cgi', $cgi, undef, $search->query);
    if ($table->view_count > $query_limit){
        print $cgi->multipart_end if $serverpush;
        ThrowUserError('testopia-query-too-large', {'limit' => $query_limit});
    }
    
    if ($table->list_count > 0){
        my $plan_id = $table->list->[0]->plan->product_id;
        foreach my $run (@{$table->list}){
            if ($run->plan->product_id != $plan_id){
                $vars->{'multiprod'} = 1;
                last;
            }
        }
        if (!$vars->{'multiprod'}) {
            my $p = $table->list->[0]->plan;
            my $build_list  = $p->product->builds;
            unshift @{$build_list},  {'id' => -1, 'name' => "--Do Not Change--"};
            $vars->{'build_list'} = $build_list;
        }
        
    }        
    my $r = Bugzilla::Testopia::TestRun->new({'run_id' => 0 });

    my $status_list = $r->get_status_list;
    
    unshift @{$status_list}, {'id' => -1, 'name' => "--Do Not Change--"};

    $vars->{'status_list'} = $status_list;
    
    $vars->{'fullwidth'} = 1; #novellonly
    $vars->{'dotweak'} = UserInGroup('Testers');
    $vars->{'table'} = $table;
    if ($serverpush && !$cgi->param('debug')) {
        print $cgi->multipart_end;
        print $cgi->multipart_start;
    }
    $template->process("testopia/run/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error()); 
    print $cgi->multipart_final if $serverpush;
}
