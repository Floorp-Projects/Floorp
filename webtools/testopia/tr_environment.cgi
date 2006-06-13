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
use Bugzilla::Config;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::TestRun;
use Bugzilla::Testopia::Environment;

Bugzilla->login(LOGIN_REQUIRED);

my $cgi = Bugzilla->cgi;

use vars qw($vars $template);
my $template = Bugzilla->template;

print $cgi->header;

my $action = $cgi->param('action') || '';
my $env_id = $cgi->param('env_id') || 0;

if ($action eq 'add'){
    my $env = Bugzilla::Testopia::Environment->new({'environment_id' => 0});
    ThrowUserError("testopia-read-only", {'object' => 'Environment'}) unless $env->canedit;
    $vars->{'environment'} = $env;
    $vars->{'action'} = "do_add";
    $template->process("testopia/environment/add.html.tmpl", $vars)
        || print $template->error();
}

elsif ($action eq 'do_add'){
    my $name = $cgi->param('name');
    my $os   = $cgi->param('op_sys');
    my $platform = $cgi->param('rep_platform');
    my $xml = $cgi->param('xml');
    detaint_natural($os);
    detaint_natural($platform);
    trick_taint($name);
    trick_taint($xml);
    validate_selection( $os, 'id', 'op_sys');
    validate_selection($platform, 'id', 'rep_platform');
    
    my $env = Bugzilla::Testopia::Environment->new(
        { 'name'            => $name,
          'op_sys_id'       => $os,
          'rep_platform_id' => $platform,
          'xml'             => $xml 
        });
    ThrowUserError("testopia-read-only", {'object' => 'Environment'}) unless $env->canedit;
    my $success = $env->store;
    unless ($success){
        $vars->{'tr_error'} = "Environment Name $name already taken";
        $vars->{'environment'} = $env;
        $template->process("testopia/environment/add.html.tmpl", $vars)
            || print $template->error();
        exit;
    }
    $vars->{'tr_message'} = "Environment $name Added";
    display_list();
}

elsif ($action eq 'edit'){
    my $env = Bugzilla::Testopia::Environment->new($env_id);
    ThrowUserError("testopia-read-only", {'object' => 'Environment'}) unless $env->canedit;
    $vars->{'action'} = 'do_edit';
    $vars->{'environment'} = $env;
    $template->process("testopia/environment/show.html.tmpl", $vars)
        || print $template->error();
}

elsif ($action eq 'do_edit'){
    my $name = $cgi->param('name');
    my $os   = $cgi->param('op_sys');
    my $platform = $cgi->param('rep_platform');
    my $xml = $cgi->param('xml');
    detaint_natural($os);
    detaint_natural($platform);
    trick_taint($name);
    trick_taint($xml);
    validate_selection( $os, 'id', 'op_sys');
    validate_selection($platform, 'id', 'rep_platform');
    my $env = Bugzilla::Testopia::Environment->new($env_id);
    ThrowUserError("testopia-read-only", {'object' => 'Environment'}) unless $env->canedit;
    my %newvalues = 
        ( 'name'            => $name,
          'op_sys_id'       => $os,
          'rep_platform_id' => $platform,
          'xml'             => $xml 
        );
    my $success = $env->update(\%newvalues);
    unless ($success){
        $vars->{'tr_error'} = "Environment Name $name already taken";
        $vars->{'environment'} = $env;
        $template->process("testopia/environment/show.html.tmpl", $vars)
            || print $template->error();
        exit;
    }
    $vars->{'tr_message'} = "Environment $name Updated";
    display_list();

}

elsif ($action eq 'delete'){
    my $env = Bugzilla::Testopia::Environment->new($env_id);
    ThrowUserError('testopia-no-delete', {'object' => 'Environment'}) unless Param("allow-test-deletion");
    ThrowUserError("testopia-read-only", {'object' => 'Environment'}) unless $env->canedit;
    $vars->{'environment'} = $env;
    $template->process("testopia/environment/delete.html.tmpl", $vars)
        || print $template->error();

}

elsif ($action eq 'do_delete'){
    my $env = Bugzilla::Testopia::Environment->new($env_id);
    ThrowUserError('testopia-no-delete', {'object' => 'Environment'}) unless Param("allow-test-deletion");
    ThrowUserError("testopia-read-only", {'object' => 'Environment'}) unless $env->canedit;
    ThrowUserError("testopia-non-zero-run-count", {'object' => 'Environment'}) if $env->get_run_count;
    $env->obliterate;
    $vars->{'tr_message'} = "Environment Deleted";
    display_list();
}

else { 
    display_list();
}

sub display_list {
    my $dbh = Bugzilla->dbh;
    my $ref = $dbh->selectcol_arrayref("SELECT environment_id FROM test_environments");
    my @envs;
    foreach my $id (@{$ref}){
        push @envs, Bugzilla::Testopia::Environment->new($id);
    }
    $vars->{'environments'} = \@envs;
    $template->process("testopia/environment/list.html.tmpl", $vars)
        || print $template->error();
}
