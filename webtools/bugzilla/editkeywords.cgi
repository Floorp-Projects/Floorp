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
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Terry Weissman.
# Portions created by Terry Weissman are
# Copyright (C) 2000 Terry Weissman. All
# Rights Reserved.
#
# Contributor(s): Terry Weissman <terry@mozilla.org>

use strict;
use lib ".";

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::Keyword;

my $cgi = Bugzilla->cgi;
my $dbh = Bugzilla->dbh;
my $template = Bugzilla->template;
my $vars = {};

#
# Preliminary checks:
#

my $user = Bugzilla->login(LOGIN_REQUIRED);

print $cgi->header();

$user->in_group('editkeywords')
  || ThrowUserError("auth_failure", {group  => "editkeywords",
                                     action => "edit",
                                     object => "keywords"});

my $action = trim($cgi->param('action')  || '');
my $key_id = $cgi->param('id');
$vars->{'action'} = $action;


if ($action eq "") {
    $vars->{'keywords'} = Bugzilla::Keyword->get_all_with_bug_count();

    print $cgi->header();
    $template->process("admin/keywords/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());

    exit;
}
    

if ($action eq 'add') {
    print $cgi->header();

    $template->process("admin/keywords/create.html.tmpl", $vars)
      || ThrowTemplateError($template->error());

    exit;
}

#
# action='new' -> add keyword entered in the 'action=add' screen
#
if ($action eq 'new') {
    my $name = $cgi->param('name') || '';
    my $desc = $cgi->param('description')  || '';

    my $keyword = Bugzilla::Keyword->create(
        { name => $name, description => $desc });

    print $cgi->header();

    $vars->{'name'} = $keyword->name;
    $template->process("admin/keywords/created.html.tmpl", $vars)
      || ThrowTemplateError($template->error());

    exit;
}

    

#
# action='edit' -> present the edit keywords from
#
# (next action would be 'update')
#

if ($action eq 'edit') {
    my $keyword = new Bugzilla::Keyword($key_id)
        || ThrowCodeError('invalid_keyword_id', { id => $key_id });

    $vars->{'keyword'} = $keyword;

    print $cgi->header();
    $template->process("admin/keywords/edit.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}


#
# action='update' -> update the keyword
#

if ($action eq 'update') {
    my $keyword = new Bugzilla::Keyword($key_id)
        || ThrowCodeError('invalid_keyword_id', { id => $key_id });

    $keyword->set_name($cgi->param('name'));
    $keyword->set_description($cgi->param('description'));
    $keyword->update();

    print $cgi->header();

    $vars->{'keyword'} = $keyword;
    $template->process("admin/keywords/rebuild-cache.html.tmpl", $vars)
      || ThrowTemplateError($template->error());

    exit;
}


if ($action eq 'delete') {
    my $keyword =  new Bugzilla::Keyword($key_id)
        || ThrowCodeError('invalid_keyword_id', { id => $key_id });

    $vars->{'keyword'} = $keyword;

    if (!$cgi->param('reallydelete') && $keyword->bug_count) {
        print $cgi->header();
        $template->process("admin/keywords/confirm-delete.html.tmpl", $vars)
            || ThrowTemplateError($template->error());
        exit;
    }

    $dbh->do('DELETE FROM keywords WHERE keywordid = ?', undef, $keyword->id);
    $dbh->do('DELETE FROM keyworddefs WHERE id = ?', undef, $keyword->id);

    print $cgi->header();

    $template->process("admin/keywords/rebuild-cache.html.tmpl", $vars)
      || ThrowTemplateError($template->error());

    exit;
}

ThrowCodeError("action_unrecognized", $vars);
