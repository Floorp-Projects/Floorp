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
use Bugzilla::Testopia::TestCase;
use Bugzilla::Testopia::TestCaseRun;

use vars qw($template $vars);

# These are going away after 2.22
require "globals.pl";
require "CGI.pl";

Bugzilla->login();
print Bugzilla->cgi->header();
   
my $dbh = Bugzilla->dbh;
my $cgi = Bugzilla->cgi;

my $caserun_id = Bugzilla->cgi->param('caserun_id');
validate_test_id($caserun_id, 'case_run');

ThrowUserError('testopia-missing-parameter', {'param' => 'caserun_id'}) unless ($caserun_id);
push @{$::vars->{'style_urls'}}, 'testopia/css/default.css';

my $action = $cgi->param('action') || '';

if ($action eq 'Commit'){
    Bugzilla->login(LOGIN_REQUIRED);
    my $caserun = Bugzilla::Testopia::TestCaseRun->new($caserun_id);
    ThrowUserError("testopia-read-only", {'object' => 'case run'}) if !$caserun->canedit;

    my $status   = $cgi->param('status');
    my $notes    = $cgi->param('notes');
    my $build    = $cgi->param('caserun_build');
    my $assignee = DBNameToIdAndCheck(trim($cgi->param('assignee')));
    
    my @buglist;
    foreach my $bug (split(/[\s,]+/, $cgi->param('bugs'))){
        ValidateBugID($bug);
        push @buglist, $bug;
    }
    
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
        'notes'    => $notes,
    );
    
    $caserun = Bugzilla::Testopia::TestCaseRun->new($caserun->update(\%newfields));
    $vars->{'caserun'} = $caserun;
    $template->process("testopia/caserun/show.html.tmpl", $vars) ||
        ThrowTemplateError($template->error());
}
####################
### Ajax Actions ###
####################
elsif ($action eq 'update_build'){
    Bugzilla->login(LOGIN_REQUIRED);
    my $caserun = Bugzilla::Testopia::TestCaseRun->new($caserun_id);
    print "Error - You don't have permission" if !$caserun->canedit;
    my $build_id = $cgi->param('build_id');
    detaint_natural($build_id);

    my $cid;
    if ($caserun->status ne 'IDLE'){
        my ($is) = $dbh->selectrow_array(
                "SELECT case_run_id 
                   FROM test_case_runs 
                  WHERE run_id = ? AND build_id = ?",
                  undef, ($caserun->run->id, $build_id));
        if ($is){
            $caserun = Bugzilla::Testopia::TestCaseRun->new($is);
        }
        else { 
            $cid = $caserun->clone({'build_id' => $build_id });
            $caserun = Bugzilla::Testopia::TestCaseRun->new($cid);
        }
    }
    else {
        $caserun->set_build($build_id );
    }
    print $caserun->status ."|". $caserun->close_date ."|". $caserun->testedby->login;
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
    $caserun->set_status($status_id);
    $caserun = Bugzilla::Testopia::TestCaseRun->new($caserun_id);
    print $caserun->status ."|". $caserun->close_date ."|". $caserun->testedby->login;
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
    $caserun->set_note($note);
}
elsif ($action eq 'update_assignee'){
    Bugzilla->login(LOGIN_REQUIRED);
    my $caserun = Bugzilla::Testopia::TestCaseRun->new($caserun_id);
    if (!$caserun->canedit){
        print "Error - You don't have permission";
        exit;
    }
    my $assignee_id = DBNameToIdAndCheck(trim($cgi->param('assignee')));
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
        ValidateBugID($bug);
        push @buglist, $bug;
    }
    foreach my $bug (@buglist){
        $caserun->attach_bug($bug);
    }
    foreach my $bug (@{$caserun->bugs}){
        print &::GetBugLink($bug->bug_id, $bug->bug_id) ." ";
    }
}
else {
    my $caserun = Bugzilla::Testopia::TestCaseRun->new($caserun_id);
    ThrowUserError('insufficient-view-perms') if !$caserun->canview;
    $vars->{'caserun'} = $caserun;
    $vars->{'action'} = 'tr_show_caserun.cgi';
    $template->process("testopia/caserun/show.html.tmpl", $vars) ||
        ThrowTemplateError($template->error());
}
