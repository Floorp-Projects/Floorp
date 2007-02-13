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
my $plan_id = $cgi->param('plan_id');
my $case_id = $cgi->param('case_id');
my $caserun_id = $cgi->param('caserun_id');

detaint_natural($attach_id);
detaint_natural($plan_id);
detaint_natural($case_id);
detaint_natural($caserun_id);

unless ($attach_id && ($plan_id || $case_id || $caserun_id)){
    print $cgi->header();
    $template->process("testopia/attachment/choose.html.tmpl", $vars) 
        || ThrowTemplateError($template->error());
    exit;
}

my $obj;
if ($plan_id){
    $obj = Bugzilla::Testopia::TestPlan->new($plan_id);
}
elsif ($case_id){
    $obj = Bugzilla::Testopia::TestCase->new($case_id);
}
elsif ($caserun_id){
    $obj = Bugzilla::Testopia::TestCaseRun->new($caserun_id);
}
unless ($obj->canview){
    print $cgi->header;
    ThrowUserError('testopia-permission-denied', {'object' => 'Attachment'});
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
    $vars->{'obj'} = $obj;
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

    print $cgi->header();
    $vars->{'attachment'} = $attachment;
    $vars->{'tr_message'} = "Attachment updated";
    $vars->{'backlink'} = $obj;
    $vars->{'obj'} = $obj;
    $vars->{'isviewable'} = $attachment->isViewable($cgi);
    $template->process("testopia/attachment/show.html.tmpl", $vars)
        || ThrowTemplateError($template->error());

}
####################
###    Delete    ###
####################
elsif ($action eq 'delete') {
    Bugzilla->login(LOGIN_REQUIRED);
    my $attachment = validate();
    ThrowUserError('testopia-no-delete', {'object' => 'Attachment'}) unless $attachment->candelete;
    $vars->{'attachment'} = validate();
    $vars->{'obj'} = $obj;
    print $cgi->header();
    $template->process("testopia/attachment/delete.html.tmpl", $vars)
        || ThrowTemplateError($template->error());
    
}
elsif ($action eq 'do_delete') {
    Bugzilla->login(LOGIN_REQUIRED);
    my $attachment = validate();
    $vars->{'tr_message'} = "Attachment ". $attachment->description ." deleted";
    ThrowUserError('testopia-no-delete', {'object' => 'Attachment'}) unless $attachment->candelete;
    if ($plan_id){
        $attachment->unlink_plan($plan_id);
    }
    elsif ($case_id){
        $attachment->unlink_plan($case_id);
    }

    print $cgi->header();
    $vars->{'tr_message'} = "Attachment deleted";
    $vars->{'backlink'} = $obj;
    $vars->{'deleted'} = 1;
    $template->process("testopia/attachment/delete.html.tmpl", $vars)
        || ThrowTemplateError($template->error());

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
    ThrowUserError('testopia-read-only', {'object' => 'Case'}) unless $obj->canedit;
    return $attachment;
}
