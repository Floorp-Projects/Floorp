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
use Bugzilla::Bug;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Util;
use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::Search;
use Bugzilla::Testopia::Table;
use Bugzilla::Testopia::TestRun;
use Bugzilla::Testopia::TestCase;
use Bugzilla::Testopia::TestCaseRun;

use vars qw($template $vars);
my $template = Bugzilla->template;
my $query_limit = 15000;
# These are going away after 2.22
require "globals.pl";

Bugzilla->login();
Bugzilla->batch(1);
print Bugzilla->cgi->header();
   
my $dbh = Bugzilla->dbh;
my $cgi = Bugzilla->cgi;

my $caserun_id = Bugzilla->cgi->param('caserun_id');
validate_test_id($caserun_id, 'case_run');

ThrowUserError('testopia-missing-parameter', {'param' => 'caserun_id'}) unless ($caserun_id);
push @{$::vars->{'style_urls'}}, 'testopia/css/default.css';

my $action = $cgi->param('action') || '';

# For use on the classic form

if ($action eq 'Commit'){
    Bugzilla->login(LOGIN_REQUIRED);
    my $caserun = Bugzilla::Testopia::TestCaseRun->new($caserun_id);
    ThrowUserError("testopia-read-only", {'object' => 'case run'}) if !$caserun->canedit;

    my $status   = $cgi->param('status');
    my $notes    = $cgi->param('notes');
    my $build    = $cgi->param('caserun_build');
    my $env      = $cgi->param('caserun_env');
    my $confirm  = $cgi->param('confirm');
    my $assignee = DBNameToIdAndCheck(trim($cgi->param('assignee')));
    
    validate_test_id($build, 'build');
    validate_test_id($env, 'environment');
    my @buglist;
    foreach my $bug (split(/[\s,]+/, $cgi->param('bugs'))){
        ValidateBugID($bug);
        push @buglist, $bug;
    }
    
    detaint_natural($env);
    detaint_natural($build);
    detaint_natural($status);
    trick_taint($notes);
        
    my %newfields = (
        'assignee' => $assignee,
        'testedby' => Bugzilla->user->id,
        'case_run_status_id' => $status,
        'build_id' => $build,
        'environment_id' => $env,
    );
    
    my $is = $caserun->check_exists($caserun->run_id, $caserun->case_id, $build, $env);
    my $existing = Bugzilla::Testopia::TestCaseRun->new($is) if $is;
    if (($build != $caserun->build->id || $env != $caserun->environment->id)
        && $is 
          && $existing->status ne 'IDLE' 
            && !$confirm){
        my $statuslist = $caserun->get_status_list;
        my $status;
        foreach my $stat (@$statuslist){
            if ($stat->{'id'} == $cgi->param('status')){
                $status = $stat->{'name'};
                last;
            }
        }
        $vars->{'existing'} = $existing;
        $vars->{'assignee'} = $cgi->param('assignee');
        $vars->{'status_name'}   = $status;
        $vars->{'status'}   = $cgi->param('status');
        $vars->{'notes'}    = $notes;
        if ($caserun->is_closed_status($cgi->param('status'))){
            $vars->{'close_date'} = get_time_stamp();
            $vars->{'testedby'} = Bugzilla->user->login;
        }
        $vars->{'bugs'} = $cgi->param('bugs');
        $template->process("testopia/caserun/confirm.html.tmpl", $vars) ||
            ThrowTemplateError($template->error());
     
        exit;   
    }

    if ($notes){
        $caserun->append_note($notes);
    }
    
    my $oldstatus_id = $caserun->status_id; 
    if ($status != $oldstatus_id){
        my $newstatus = $caserun->lookup_status($status);
        my $oldstatus = $caserun->status();
        my $note = "Status changed from $oldstatus to $newstatus by ". Bugzilla->user->login;
        $caserun->append_note($note);
    }

    foreach my $bug (@buglist){
        $caserun->attach_bug($bug);
    }
    
    $caserun = Bugzilla::Testopia::TestCaseRun->new($caserun->update(\%newfields));
    $vars->{'tr_message'} = "Case-run updated.";
    display($caserun);
}
elsif ($action eq 'delete'){
    Bugzilla->login(LOGIN_REQUIRED);
    my $caserun = Bugzilla::Testopia::TestCaseRun->new($caserun_id);
    ThrowUserError("testopia-read-only", {'object' => 'case run'}) if !$caserun->candelete;
    $vars->{'title'} = 'Remove Test Case '. $caserun->case->id .' from Run: ' . $caserun->run->summary;
    $vars->{'bugcount'} = scalar @{$caserun->bugs};
    $vars->{'form_action'} = 'tr_show_caserun.cgi';
    $vars->{'caserun'} = $caserun;
    $template->process("testopia/caserun/delete.html.tmpl", $vars) ||
        ThrowTemplateError($template->error());
}
elsif ($action eq 'do_delete'){
    Bugzilla->login(LOGIN_REQUIRED);
    my $caserun = Bugzilla::Testopia::TestCaseRun->new($caserun_id);
    ThrowUserError("testopia-read-only", {'object' => 'case run'}) if !$caserun->candelete;
    $caserun->obliterate;
    $cgi->delete_all;
    $cgi->param('current_tab', 'case_run');
    $cgi->param('run_id', $caserun->run->id);
    my $search = Bugzilla::Testopia::Search->new($cgi);
    my $table = Bugzilla::Testopia::Table->new('case_run', 'tr_show_run.cgi', $cgi, undef, $search->query);
    ThrowUserError('testopia-query-too-large', {'limit' => $query_limit}) if $table->view_count > $query_limit;
    
    my @case_list;
    foreach my $cr (@{$table->list}){
        push @case_list, $cr->case_id;
    }
    my $case = Bugzilla::Testopia::TestCase->new({'case_id' => 0});
    $vars->{'run'} = $caserun->run;
    $vars->{'table'} = $table;
    $vars->{'case_list'} = join(",", @case_list);
    $vars->{'action'} = 'Commit';
    $template->process("testopia/run/show.html.tmpl", $vars) ||
        ThrowTemplateError($template->error());

}
####################
### Ajax Actions ###
####################
elsif ($action eq 'update_build'){
    Bugzilla->login(LOGIN_REQUIRED);
    my $caserun = Bugzilla::Testopia::TestCaseRun->new($caserun_id);
    if (!$caserun->canedit) { 
        print "Error - You don't have permission";
        exit;
    }
    my $build_id = $cgi->param('build_id');
    detaint_natural($build_id);
    validate_test_id($build_id, 'build');
    my $is = $caserun->check_exists($caserun->run_id, $caserun->case_id, $build_id, $caserun->environment->id);
    if ($is){
        $caserun = Bugzilla::Testopia::TestCaseRun->new($is);
    }
    elsif ($caserun->status ne 'IDLE'){
        my $cid = $caserun->clone({'build_id' => $build_id });
        $caserun = Bugzilla::Testopia::TestCaseRun->new($cid);
    }
    else {
        $caserun->set_build($build_id );
    }
    my $body_data;
    my $head_data;
    $vars->{'caserun'} = $caserun;
    $vars->{'index'}   = $cgi->param('index');
    $template->process("testopia/caserun/short-form-header.html.tmpl", $vars, \$head_data) ||
        ThrowTemplateError($template->error());
    $template->process("testopia/caserun/short-form.html.tmpl", $vars, \$body_data) ||
        ThrowTemplateError($template->error());
    print $head_data . "|~+" . $body_data;
}
elsif ($action eq 'update_environment'){
    Bugzilla->login(LOGIN_REQUIRED);
    my $caserun = Bugzilla::Testopia::TestCaseRun->new($caserun_id);
    if (!$caserun->canedit) { 
        print "Error - You don't have permission";
        exit;
    }
    my $environment_id = $cgi->param('caserun_env');
    detaint_natural($environment_id);
    validate_test_id($environment_id, 'environment');
    my $is = $caserun->check_exists($caserun->run_id, $caserun->case_id, $caserun->build->id, $environment_id);
    if ($is){
        $caserun = Bugzilla::Testopia::TestCaseRun->new($is);
    }
    elsif ($caserun->status ne 'IDLE'){
        my $cid = $caserun->clone({'environment_id' => $environment_id });
        $caserun = Bugzilla::Testopia::TestCaseRun->new($cid);
    }
    else {
        $caserun->set_environment($environment_id );
    }
    my $body_data;
    my $head_data;
    $vars->{'caserun'} = $caserun;
    $vars->{'index'}   = $cgi->param('index');
    $template->process("testopia/caserun/short-form-header.html.tmpl", $vars, \$head_data) ||
        ThrowTemplateError($template->error());
    $template->process("testopia/caserun/short-form.html.tmpl", $vars, \$body_data) ||
        ThrowTemplateError($template->error());
    print $head_data . "|~+" . $body_data;
}
elsif ($action eq 'update_status'){
    Bugzilla->login(LOGIN_REQUIRED);
    my $caserun = Bugzilla::Testopia::TestCaseRun->new($caserun_id);
    if (!$caserun->canedit){
        print "Error - You don't have permission";
        exit;
    }
    my $status_id = $cgi->param('status_id');
    detaint_natural($status_id);
    if ($status_id != $caserun->status_id){
        my $oldstatus = $caserun->status(); 
        $caserun->set_status($status_id);
        my $newstatus = $caserun->status();
        my $note = "Status changed from $oldstatus to $newstatus by ". Bugzilla->user->login;
        $note .= " for build: '". $caserun->build->name; 
        $note .= "' and environment: '". $caserun->environment->name ."'.";
        $caserun->append_note($note);
    }
    print $caserun->status ."|". $caserun->close_date ."|". $caserun->testedby->login;
    if ($caserun->updated_deps) {
        print "|". join(',', @{$caserun->updated_deps});
    }
}
elsif ($action eq 'update_note'){
    Bugzilla->login(LOGIN_REQUIRED);
    my $caserun = Bugzilla::Testopia::TestCaseRun->new($caserun_id);
    if (!$caserun->canedit){
        print "Error - You don't have permission";
        exit;
    }
    my $note = $cgi->param('note');
    trick_taint($note);
    $caserun->append_note($note);
    print '<pre>' .  $caserun->notes . '</pre>';
}
elsif ($action eq 'update_assignee'){
    Bugzilla->login(LOGIN_REQUIRED);
    my $caserun = Bugzilla::Testopia::TestCaseRun->new($caserun_id);
    if (!$caserun->canedit){
        print "Error - You don't have permission";
        exit;
    }
    my $assignee_id = login_to_id(trim($cgi->param('assignee')));
    if ($assignee_id == 0){
        print "Error - Invalid assignee";
        exit;
    }
    $caserun->set_assignee($assignee_id);
}
elsif ($action eq 'attach_bug'){
    Bugzilla->login(LOGIN_REQUIRED);
    my $caserun = Bugzilla::Testopia::TestCaseRun->new($caserun_id);
    if (!$caserun->canedit){
        print "Error - You don't have permission";
        exit;
    }
    my @buglist;
    foreach my $bug (split(/[\s,]+/, $cgi->param('bugs'))){
        eval{
            ValidateBugID($bug);
        };
        if ($@){
            print "<span style='font-weight:bold; color:#FF0000;'>Error - Invalid bug id or alias</span>";
            exit;
        }
        push @buglist, $bug;
    }
    foreach my $bug (@buglist){
        $caserun->attach_bug($bug);
    }
    foreach my $bug (@{$caserun->case->bugs}){
        print &::GetBugLink($bug->bug_id, $bug->bug_id) ." ";
    }
}
elsif ($action eq 'detach_bug'){
    Bugzilla->login(LOGIN_REQUIRED);
    my $caserun = Bugzilla::Testopia::TestCaseRun->new($caserun_id);
    if (!$caserun->canedit){
        print "Error - You don't have permission";
        exit;
    }
    my @buglist;
    foreach my $bug (split(/[\s,]+/, $cgi->param('bug_id'))){
        ValidateBugID($bug);
        push @buglist, $bug;
    }
    foreach my $bug (@buglist){
        $caserun->detach_bug($bug);
    }
    display(Bugzilla::Testopia::TestCaseRun->new($caserun_id));
}
else {
    display(Bugzilla::Testopia::TestCaseRun->new($caserun_id));
}

sub display {
    my $caserun = shift;
    ThrowUserError('insufficient-view-perms') if !$caserun->canview;
    my $table = Bugzilla::Testopia::Table->new('case_run');
    ThrowUserError('testopia-query-too-large', {'limit' => $query_limit}) if $table->list_count > $query_limit;
    $vars->{'table'} = $table;
    $vars->{'caserun'} = $caserun;
    $vars->{'action'} = 'tr_show_caserun.cgi';
    $template->process("testopia/caserun/show.html.tmpl", $vars) ||
        ThrowTemplateError($template->error());
}