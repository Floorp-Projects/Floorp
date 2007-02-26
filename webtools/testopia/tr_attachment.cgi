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
use Bugzilla::Util;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::Attachment;
use Bugzilla::Testopia::Search;
use Bugzilla::Testopia::Table;

use vars qw($vars);
my $template = Bugzilla->template;
my $cgi = Bugzilla->cgi;
my $dbh = Bugzilla->dbh;

Bugzilla->login(LOGIN_REQUIRED);

push @{$::vars->{'style_urls'}}, 'testopia/css/default.css';

my $action     = $cgi->param('action') || '';
my $attach_id  = $cgi->param('attach_id');
my $plan_id    = $cgi->param('plan_id');
my $case_id    = $cgi->param('case_id');
my $caserun_id = $cgi->param('caserun_id');

detaint_natural($attach_id) if $attach_id;

unless ($attach_id){
    print $cgi->header();
    $template->process("testopia/attachment/choose.html.tmpl", $vars) 
        || ThrowTemplateError($template->error());
    exit;
}

validate_test_id($attach_id,'attachment');
my $attachment = Bugzilla::Testopia::Attachment->new($attach_id);

my $obj;
if ($plan_id){
    detaint_natural($plan_id);
    $obj = Bugzilla::Testopia::TestPlan->new($plan_id);
}
elsif ($case_id){
    detaint_natural($case_id);
    $obj = Bugzilla::Testopia::TestCase->new($case_id);
}
elsif ($caserun_id){
    detaint_natural($caserun_id);
    $obj = Bugzilla::Testopia::TestCaseRun->new($caserun_id);
}

##################
###    Edit    ###
##################
if ($action eq 'edit'){
    print $cgi->header;
    ThrowUserError('testopia-permission-denied', {'object' => 'Attachment'}) unless $attachment->canedit;
    
    $vars->{'attachment'} = $attachment;
    $vars->{'isviewable'} = $attachment->isViewable($cgi);
    $vars->{'obj'} = $obj;
    $template->process("testopia/attachment/show.html.tmpl", $vars)
        || ThrowTemplateError($template->error());
}
elsif ($action eq 'do_edit') {
    print $cgi->header;
    ThrowUserError('testopia-permission-denied', {'object' => 'Attachment'}) unless $attachment->canedit;

    my %newvalues = ( 
        'description' => $cgi->param('description') || '',
        'filename'    => $cgi->param('filename'),
        'mime_type'   => $cgi->param('mime_type'),
    );
    $attachment->update(\%newvalues);

    $vars->{'attachment'} = $attachment;
    $vars->{'tr_message'} = "Attachment updated";
    $vars->{'backlink'} = $obj;
    $vars->{'obj'} = $obj;
    $vars->{'isviewable'} = $attachment->isViewable($cgi);
    $template->process("testopia/attachment/show.html.tmpl", $vars)
        || ThrowTemplateError($template->error());

}

####################
###    Unlink    ###
####################

elsif ($action eq 'remove') {
    print $cgi->header;
    ThrowUserError('testopia-missing-parameter', {'param' => 'case_id or plan_id'}) unless $obj;
    ThrowUserError('testopia-no-delete', {'object' => 'Attachment'}) unless $obj->canedit;
    $vars->{'attachment'} = $attachment;
    $vars->{'action'} = 'do_remove';
    $vars->{'obj'} = $obj;
    
    $template->process("testopia/attachment/delete.html.tmpl", $vars)
        || ThrowTemplateError($template->error());
    
}
elsif ($action eq 'do_remove') {
    print $cgi->header;
    $vars->{'tr_message'} = "Attachment ". $attachment->description ." deleted";
    ThrowUserError('testopia-missing-parameter', {'param' => 'case_id or plan_id'}) unless $obj;
    ThrowUserError('testopia-no-delete', {'object' => 'Attachment'}) unless $obj->canedit;
    if ($plan_id){
        $attachment->unlink_plan($plan_id);
    }
    elsif ($case_id){
        $attachment->unlink_plan($case_id);
    }

    $vars->{'tr_message'} = "Attachment removed";
    $vars->{'backlink'} = $obj;
    $vars->{'deleted'} = 1;
    $template->process("testopia/attachment/delete.html.tmpl", $vars)
        || ThrowTemplateError($template->error());

}

####################
###    Delete    ###
####################

elsif ($action eq 'delete') {
    print $cgi->header;
    ThrowUserError('testopia-no-delete', {'object' => 'Attachment'}) unless $attachment->candelete;
    $vars->{'attachment'} = $attachment;
    $vars->{'action'} = 'do_delete';
    
    $template->process("testopia/attachment/delete.html.tmpl", $vars)
        || ThrowTemplateError($template->error());
    
}
elsif ($action eq 'do_delete') {
    print $cgi->header;
    $vars->{'tr_message'} = "Attachment ". $attachment->description ." deleted";
    ThrowUserError('testopia-no-delete', {'object' => 'Attachment'}) unless $attachment->candelete;
    
    $attachment->obliterate;
    $vars->{'tr_message'} = "Attachment deleted";
    $vars->{'deleted'} = 1;
    $template->process("testopia/attachment/delete.html.tmpl", $vars)
        || ThrowTemplateError($template->error());

}
################
###   View   ###
################
else {
    
    my $filename = $attachment->filename;
    $filename =~ s/\\/\\\\/g; # escape backslashes
    $filename =~ s/"/\\"/g; # escape quotes
    
    print $cgi->header(-type => $attachment->mime_type . "; name=\"$filename\"",
                       -content_disposition => "inline; filename=\"$filename\"",
                       -content_length      => $attachment->datasize);

    print $attachment->contents;
}
