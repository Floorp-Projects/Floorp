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
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::Constants;
use Bugzilla::Testopia::Search;
use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::TestCaseRun;
use Bugzilla::Testopia::Table;

use vars qw($vars $template);
require "globals.pl";
require "CGI.pl";
my $dbh = Bugzilla->dbh;
my $cgi = Bugzilla->cgi;
my $template = Bugzilla->template;

push @{$::vars->{'style_urls'}}, 'testopia/css/default.css';

$cgi->send_cookie(-name => "TEST_LAST_ORDER",
                  -value => $cgi->param('order'),
                  -expires => "Fri, 01-Jan-2038 00:00:00 GMT");
Bugzilla->login();
print $cgi->header;
my $action = $cgi->param('action') || '';

if ($action eq 'Commit'){
    Bugzilla->login(LOGIN_REQUIRED);
    # Get the list of checked items. This way we don't have to cycle through 
    # every test case, only the ones that are checked.
    my $reg = qr/r_([\d]+)/;
    my @caseruns;
    foreach my $p ($cgi->param()){
        push @caseruns, Bugzilla::Testopia::TestCaseRun->new($1) if $p =~ $reg;
    }
    ThrowUserError('testopia-none-selected', {'object' => 'case-run'}) if (scalar @caseruns < 1);
    my @buglist;
    foreach my $bug (split(/[\s,]+/, $cgi->param('bugs'))){
        ValidateBugID($bug);
        push @buglist, $bug;
    }

    foreach my $caserun (@caseruns){
    
        ThrowUserError("testopia-read-only", {'object' => 'Case Run'}) unless $caserun->canedit;
        my $status   = $cgi->param('status') == -1 ? $caserun->status_id : $cgi->param('status');
        my $build    = $cgi->param('caserun_build') == -1 ? $caserun->build->id : $cgi->param('caserun_build');
        my $assignee = $cgi->param('assignee') eq '--Do Not Change--' ? $caserun->assignee->id : DBNameToIdAndCheck(trim($cgi->param('assignee')));
        my $notes    = $cgi->param('notes');        
        
        detaint_natural($build);
        detaint_natural($status);
        trick_taint($notes);
    
        foreach my $bug (@buglist){
            $caserun->attach_bug($bug);
        }
    
        my %newfields = (
            'assignee' => $assignee,
            'testedby' => Bugzilla->user->id,
            'case_run_status_id' => $status,
            'build_id' => $build,
        );
        $caserun->update(\%newfields);
        $caserun->set_note($notes, 1);
    }
    $vars->{'title'} = "Update Susccessful";
    $vars->{'tr_message'} = scalar @caseruns . ' Test Case-Runs Updated';
    
    if ($cgi->param('run_id')){
        my $run_id = $cgi->param('run_id');
        my $run = Bugzilla::Testopia::TestRun->new($run_id);
        $cgi->delete_all;
        $cgi->param('run_id', $run_id);
        $cgi->param('current_tab', 'case_run');
        my $search = Bugzilla::Testopia::Search->new($cgi);
        my $table = Bugzilla::Testopia::Table->new('case_run', 'tr_show_run.cgi', $cgi, undef, $search->query);
    
        $vars->{'run'} = $run;
        $vars->{'table'} = $table;
        $vars->{'action'} = 'Commit';
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
    exit;
}
# Take the search from the URL params and convert it to SQL
$cgi->delete('build');
$cgi->param('current_tab', 'case_run');
my $search = Bugzilla::Testopia::Search->new($cgi);
my $table = Bugzilla::Testopia::Table->new('case_run', 'tr_list_caseruns.cgi', $cgi, undef, $search->query);

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
        my $build_list  = $p->builds;
        unshift @{$build_list},  {'id' => -1, 'name' => "--Do Not Change--"};
        $vars->{'build_list'} = $build_list;
    }
    
} 
if ($cgi->param('run_id')){
    $vars->{'run'} = Bugzilla::Testopia::TestRun->new($cgi->param('run_id'));
}
$vars->{'dotweak'} = UserInGroup('edittestcases');
$vars->{'table'} = $table;
$vars->{'action'} = 'tr_list_caserun.cgi';
$template->process("testopia/caserun/list.html.tmpl", $vars)
    || ThrowTemplateError($template->error());
 
