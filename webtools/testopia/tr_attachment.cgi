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
use Bugzilla::Util;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::Attachment;
use Bugzilla::Testopia::Search;
use Bugzilla::Testopia::Table;

require "globals.pl";

my $cgi = Bugzilla->cgi;
my $dbh = Bugzilla->dbh;

Bugzilla->login();

use vars qw($vars $template);
my $template = Bugzilla->template;

push @{$::vars->{'style_urls'}}, 'testopia/css/default.css';
my $action  = $cgi->param('action') || '';
my $attach_id = $cgi->param('attach_id');

unless ($attach_id){
    print $cgi->header();
    $template->process("testopia/attachment/choose.html.tmpl", $vars) 
        || ThrowTemplateError($template->error());
    exit;
}
##################
###    Edit    ###
##################
if ($action eq 'edit'){
    Bugzilla->login(LOGIN_REQUIRED);
    my $attachment = validate();
    $vars->{'attachment'} = validate();
    $vars->{'isviewable'} = $attachment->isViewable($cgi);
    print $cgi->header();
    $template->process("testopia/attachment/show.html.tmpl", $vars)
        || ThrowTemplateError($template->error());
}
elsif ($action eq 'do_edit') {
    Bugzilla->login(LOGIN_REQUIRED);
    my $attachment = validate();
    my %newvalues = ( 
        'description' => $cgi->param('description') || '',
        'filename'    => $cgi->param('filename'),
        'mime_type'   => $cgi->param('mime_type'),
    );
    $attachment->update(\%newvalues);
    $vars->{'tr_message'} = "Attachment updated";
    go_on($attachment);
}
####################
###    Delete    ###
####################
elsif ($action eq 'delete') {
    Bugzilla->login(LOGIN_REQUIRED);
    my $attachment = validate();
    ThrowUserError('testopia-no-delete', {'object' => 'Attachment'}) unless Param("allow-test-deletion");
    $vars->{'attachment'} = validate();
    print $cgi->header();
    $template->process("testopia/attachment/delete.html.tmpl", $vars)
        || ThrowTemplateError($template->error());
    
}
elsif ($action eq 'do_delete') {
    Bugzilla->login(LOGIN_REQUIRED);
    my $attachment = validate();
    $vars->{'tr_message'} = "Attachment ". $attachment->description ." deleted";
    ThrowUserError('testopia-no-delete', {'object' => 'Attachment'}) unless Param("allow-test-deletion");
    $attachment->obliterate;
    go_on($attachment);
}
################
###   View   ###
################
else {
    Bugzilla->login();
    validate_test_id($attach_id,'attachment');
    my $attachment = Bugzilla::Testopia::Attachment->new($attach_id);
    my $filename = $attachment->filename;
    $filename =~ s/\\/\\\\/g; # escape backslashes
    $filename =~ s/"/\\"/g; # escape quotes
    
    print $cgi->header(-type => $attachment->mime_type . "; name=\"$filename\"",
                       -content_disposition => "inline; filename=\"$filename\"",
                       -content_length      => $attachment->datasize);

    print $attachment->contents;
}

#######################
###   Helper Subs   ###
#######################
sub validate {
    validate_test_id($attach_id,'attachment');
    my $attachment = Bugzilla::Testopia::Attachment->new($attach_id);
    if ($attachment->plan_id){
        my $plan = Bugzilla::Testopia::TestPlan->new($attachment->plan_id);
        ThrowUserError('testopia-read-only', {'object' => 'Plan'}) unless $plan->canedit;
    }
    if ($attachment->case_id){
        my $case = Bugzilla::Testopia::TestCase->new($attachment->case_id);
        ThrowUserError('testopia-read-only', {'object' => 'Case'}) unless $case->canedit;
    }
    return $attachment;
}

sub go_on {
    my $attachment = shift;
    print $cgi->header();
    $vars->{'action'} = "Commit";
    if ($attachment->plan_id){
        $vars->{'form_action'} = "tr_show_plan.cgi";
        $cgi->param('plan_id', $attachment->plan_id); 
        my $casequery = new Bugzilla::CGI($cgi);
        my $runquery  = new Bugzilla::CGI($cgi);
    
        $casequery->param('current_tab', 'case');
        my $search = Bugzilla::Testopia::Search->new($casequery);
        my $table = Bugzilla::Testopia::Table->new('case', 'tr_show_plan.cgi', $casequery, undef, $search->query);
        $vars->{'case_table'} = $table;    
      
        $runquery->param('current_tab', 'run');
        $search = Bugzilla::Testopia::Search->new($runquery);
        $table = Bugzilla::Testopia::Table->new('run', 'tr_show_plan.cgi', $runquery, undef, $search->query);
        $vars->{'run_table'} = $table;

        $vars->{'plan'} = Bugzilla::Testopia::TestPlan->new($attachment->plan_id);
        $template->process("testopia/plan/show.html.tmpl", $vars)
            || ThrowTemplateError($template->error());
    }
    else {
        $vars->{'form_action'} = "tr_show_case.cgi";
        $vars->{'case'} = Bugzilla::Testopia::TestCase->new($attachment->case_id);
        $template->process("testopia/case/show.html.tmpl", $vars)
            || ThrowTemplateError($template->error());
    }
}
