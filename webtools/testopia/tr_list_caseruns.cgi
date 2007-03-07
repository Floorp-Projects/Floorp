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
use Bugzilla::Bug;
use Bugzilla::Util;
use Bugzilla::User;
use Bugzilla::Error;
use Bugzilla::Constants;
use Bugzilla::Testopia::Search;
use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::TestCaseRun;
use Bugzilla::Testopia::Table;
use Bugzilla::Testopia::Constants;

use vars qw($vars);
require 'globals.pl';

my $cgi = Bugzilla->cgi;
my $template = Bugzilla->template;
my $query_limit = 15000;

push @{$::vars->{'style_urls'}}, 'testopia/css/default.css';

$cgi->send_cookie(-name => "TEST_LAST_ORDER",
                  -value => $cgi->param('order'),
                  -expires => "Fri, 01-Jan-2038 00:00:00 GMT");

Bugzilla->login(LOGIN_REQUIRED);

$vars->{'fullwidth'} = 1;

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
    # Get the list of checked items. This way we don't have to cycle through 
    # every test case, only the ones that are checked.
    my $reg = qr/r_([\d]+)/;
    my $params = join(" ", $cgi->param());
    my @params = $cgi->param();
    my @buglist;
    unless ($params =~ $reg){
        print $cgi->multipart_end if $serverpush;
        ThrowUserError('testopia-none-selected', {'object' => 'case-run'});
    }
    foreach my $bug (split(/[\s,]+/, $cgi->param('bugs'))){
        ValidateBugID($bug);
        push @buglist, $bug;
    }

    my $progress_interval = 250;
    my $i = 0;
    my $total = scalar @params;
    my @uneditable;
    
    foreach my $p ($cgi->param()){
        my $caserun = Bugzilla::Testopia::TestCaseRun->new($1) if $p =~ $reg;
        next unless $caserun;
        
        $i++;
        if ($i % $progress_interval == 0 && $serverpush){
            print $cgi->multipart_end;
            print $cgi->multipart_start;
            $vars->{'complete'} = $i;
            $vars->{'total'} = $total;
            $template->process("testopia/progress.html.tmpl", $vars)
              || ThrowTemplateError($template->error());
        }

        my $build    = $cgi->param('caserun_build') == -1 ? $caserun->build->id : $cgi->param('caserun_build');
        my $notes    = $cgi->param('notes');
        my $env      = $cgi->param('caserun_env') eq '' ? $caserun->environment->id : $cgi->param('caserun_env');
        
        validate_test_id($build, 'build');
        validate_test_id($env, 'environment');
        
        detaint_natural($env);
        detaint_natural($build);
        
        trick_taint($notes);
        
        unless ($caserun->canedit){
            push @uneditable, $caserun;
            next;
        }
        
        # Switch to the record representing this build and environment combo.
        # If there is not one, it will create it and switch to that.
        $caserun = $caserun->switch($build,$env);
        
        my $status   = $cgi->param('status') == -1 ? $caserun->status_id : $cgi->param('status');
        my $assignee = $cgi->param('assignee') eq '--Do Not Change--' ? $caserun->assignee->id : login_to_id(trim($cgi->param('assignee')));       
        unless ($assignee){
           print $cgi->multipart_end if $serverpush;
           ThrowUserError("invalid_username", { name => $cgi->param('assignee') });
        }
        detaint_natural($status);
        
        $caserun->set_status($status)     if ($caserun->status_id != $status);
        $caserun->set_assignee($assignee) if ($caserun->assignee->id != $assignee);
        $caserun->append_note($notes);
        
        foreach my $bug (@buglist){
            $caserun->attach_bug($bug);
        }
        
        
    }
    $vars->{'title'} = "Update Successful";
    my $updated = $i - scalar @uneditable;
    $vars->{'tr_error'} = scalar @uneditable . " Not updated" if scalar @uneditable > 0;
    $vars->{'tr_message'} = "$updated Test Case-Runs Updated";
    if ($serverpush && !$cgi->param('debug')) {
        print $cgi->multipart_end;
        print $cgi->multipart_start;
    }
    if ($cgi->param('run_id')){
        my $run_id = $cgi->param('run_id');
        my $run = Bugzilla::Testopia::TestRun->new($run_id);
        
        # See if there is a saved filter
        if ($cgi->cookie('TESTOPIA-FILTER-RUN-' . $run_id) && $action ne 'Filter' && $action ne 'clear_filter'){
            $cgi = Bugzilla::CGI->new($cgi->cookie('TESTOPIA-FILTER-RUN-' . $run_id));
            $vars->{'filtered'} = 1;
        }
        else {
            $cgi->delete_all;
        }
        $cgi->param('run_id', $run_id);
        $cgi->param('current_tab', 'case_run');
        my $search = Bugzilla::Testopia::Search->new($cgi);
        my $table = Bugzilla::Testopia::Table->new('case_run', 'tr_show_run.cgi', $cgi, undef, $search->query);
    
        $vars->{'run'} = $run;
        $vars->{'table'} = $table;
        $vars->{'action'} = 'Commit';
        $vars->{'backlink'} = $run;
        $vars->{'form_action'} = "tr_show_run.cgi";
        $vars->{'caserun'} = Bugzilla::Testopia::TestCaseRun->new({});
        $vars->{'case'} = Bugzilla::Testopia::TestCase->new({});
        
        $template->process("testopia/run/show.html.tmpl", $vars) ||
            ThrowTemplateError($template->error());
        
    }
    else {
        my $case = Bugzilla::Testopia::TestCase->new({ 'case_id' => 0 });
        $vars->{'case'} = $case;    
        $vars->{'current_tab'} = 'cases';
        $template->process("testopia/search/advanced.html.tmpl", $vars) ||
            ThrowTemplateError($template->error());
    }
    print $cgi->multipart_final if $serverpush;
    exit;
}
elsif ($action eq 'Delete Selected'){
    my $reg = qr/r_([\d]+)/;
    my @caseruns;
    foreach my $p ($cgi->param()){
        my $caserun = Bugzilla::Testopia::TestCaseRun->new($1) if $p =~ $reg;
        if (($caserun && !$caserun->candelete)){
            print $cgi->multipart_end if $serverpush;
            ThrowUserError("testopia-read-only", {'object' => 'case run'});
        }
        push @caseruns, $caserun if $caserun;
    }
    if ($serverpush) {
        print $cgi->multipart_end;
        print $cgi->multipart_start;
    }
    if ((scalar @caseruns < 1)){
        print $cgi->multipart_end if $serverpush;
        ThrowUserError('testopia-none-selected', {'object' => 'case-run'});
    }
    $vars->{'caseruns'} = \@caseruns;
    $vars->{'caseruncount'} = scalar @caseruns;
    $vars->{'title'} = "Remove Test Cases from Run"; 
    $vars->{'form_action'} = 'tr_list_caseruns.cgi';
    $vars->{'run_id'} = $cgi->param('run_id');
    $template->process("testopia/caserun/delete.html.tmpl", $vars) ||
        ThrowTemplateError($template->error());
    print $cgi->multipart_final if $serverpush;
    exit;
}
elsif ($action eq 'do_delete'){
    my @caseruns;
    foreach my $id ($cgi->param('caserun_id')){
        my $caserun = Bugzilla::Testopia::TestCaseRun->new($id);
        push @caseruns, $caserun;
        unless ($caserun->candelete){
            print $cgi->multipart_end if $serverpush;
            ThrowUserError("testopia-read-only", {'object' => 'case run'});
        }
    }
    my $progress_interval = 250;
    my $i = 0;
    my $total = scalar @caseruns;
    foreach my $c (@caseruns){
        $i++;
        if ($i % $progress_interval == 0 && $serverpush){
            print $cgi->multipart_end;
            print $cgi->multipart_start;
            $vars->{'complete'} = $i;
            $vars->{'total'} = $total;
            $template->process("testopia/progress.html.tmpl", $vars)
              || ThrowTemplateError($template->error());
        }
        $c->obliterate;
    }
    if ($serverpush) {
        print $cgi->multipart_end;
        print $cgi->multipart_start;
    }
    $vars->{'deleted'} = 1;
    $vars->{'run_id'} = $cgi->param('run_id');
    $template->process("testopia/caserun/delete.html.tmpl", $vars) ||
        ThrowTemplateError($template->error());
    print $cgi->multipart_final if $serverpush;
    exit;
}

$vars->{'qname'} = $cgi->param('qname') if $cgi->param('qname');

# Take the search from the URL params and convert it to SQL
$cgi->param('current_tab', 'case_run');
my $search = Bugzilla::Testopia::Search->new($cgi);
my $table = Bugzilla::Testopia::Table->new('case_run', 'tr_list_caseruns.cgi', $cgi, undef, $search->query);
if ($table->view_count > $query_limit){
    print $cgi->multipart_end if $serverpush;
    ThrowUserError('testopia-query-too-large', {'limit' => $query_limit});
}

if ($table->list_count > 0){
    my $prod_id = $table->list->[0]->run->plan->product_id;
    foreach my $caserun (@{$table->list}){
        if ($caserun->run->plan->product_id != $prod_id){
            $vars->{'multiprod'} = 1;
            last;
        }
    }
    if (!$vars->{'multiprod'}){
        my $p = $table->list->[0]->run->plan;
        my $build_list  = $p->product->builds;
        unshift @{$build_list},  {'id' => -1, 'name' => "--Do Not Change--"};
        $vars->{'build_list'} = $build_list;
    }
    
} 
if ($cgi->param('run_id')){
    my @runs = split(",", $cgi->param('run_id'));
    if (scalar @runs == 1){
        $vars->{'run'} = Bugzilla::Testopia::TestRun->new($cgi->param('run_id'));
        $vars->{'filtered'} = 1 if $cgi->cookie('TESTOPIA-FILTER-RUN-' . $vars->{'run'}->id) && $cgi->param('action') ne 'clear_filter';
    }
}
my $case = Bugzilla::Testopia::TestCase->new({'case_id' => 0});
$vars->{'expand_report'} = $cgi->param('expand_report') || 0;
$vars->{'expand_filter'} = $cgi->param('expand_filter') || 0;
$vars->{'table'} = $table;
$vars->{'action'} = 'tr_list_caserun.cgi';
$vars->{'caserun'} = Bugzilla::Testopia::TestCaseRun->new({});
$vars->{'case'} = Bugzilla::Testopia::TestCase->new({});
if ($vars->{'run'}) {
    $vars->{'dotweak'} = $vars->{'run'}->canedit();
    $vars->{'candelete'} = $vars->{'run'}->candelete();
}
else {
    $vars->{'dotweak'} = Bugzilla->user->in_group('Testers');
    $vars->{'candelete'} = Param('testopia-allow-group-member-deletes');
}
if ($serverpush && !$cgi->param('debug')) {
    print $cgi->multipart_end;
    print $cgi->multipart_start;
}
$template->process("testopia/caserun/list.html.tmpl", $vars)
    || ThrowTemplateError($template->error());
print $cgi->multipart_final if $serverpush; 
