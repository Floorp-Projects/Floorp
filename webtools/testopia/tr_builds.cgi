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
use Bugzilla::Config;
use Bugzilla::Testopia::TestPlan;
use Bugzilla::Testopia::Build;
use Bugzilla::Testopia::Search;
use Bugzilla::Testopia::Table;
use Bugzilla::Testopia::Util;

Bugzilla->login(LOGIN_REQUIRED);

use vars qw($vars);
my $template = Bugzilla->template;
my $cgi = Bugzilla->cgi;

print $cgi->header;

push @{$::vars->{'style_urls'}}, 'testopia/css/default.css';

my $action =  $cgi->param('action') || '';
my $product_id = $cgi->param('product_id');

ThrowUserError("testopia-missing-parameter", {param => "product_id"}) unless $product_id;
my $product = Bugzilla::Testopia::Product->new($product_id);
ThrowUserError('testopia-read-only', {'object' => 'Build'}) unless $product->canedit;

$vars->{'plan_id'} = $cgi->param('plan_id');
$vars->{'product'} = $product;   

######################
### Create a Build ###
######################
if ($action eq 'add'){
    $vars->{'action'} = 'do_add';
    $template->process("testopia/build/form.html.tmpl", $vars) 
      || ThrowTemplateError($template->error());
}

elsif ($action eq 'do_add'){
    my $cname = $cgi->param('name');
    my $desc  = $cgi->param('desc');
    my $tm    = $cgi->param('milestone');
    
    ThrowUserError('testopia-missing-required-field', {'field' => 'build name'}) unless $cname;
    
    trick_taint($cname);
    trick_taint($desc);
    trick_taint($tm);

    my $build = Bugzilla::Testopia::Build->new({
                  product_id  => $product->id,
                  name        => $cname,
                  description => $desc,
                  milestone   => $tm,
                  isactive    => $cgi->param('isactive') ? 1 : 0,
    });
    ThrowUserError('testopia-name-not-unique', 
                    {'object' => 'Build', 
                     'name' => $cname}) if $build->check_name($cname);
    $build->store;

    $vars->{'tr_message'} = "Build successfully added";
    display();
   
}

####################
### Edit a Build ###
####################
elsif ($action eq 'edit'){
    my $build = Bugzilla::Testopia::Build->new($cgi->param('build_id'));
    $vars->{'build'} = $build;
    $vars->{'action'} = 'do_edit';
    $template->process("testopia/build/form.html.tmpl", $vars) 
      || ThrowTemplateError($template->error());

}
elsif ($action eq 'do_edit'){
    my $cname = $cgi->param('name');
    my $desc  = $cgi->param('desc');
    my $milestone  = $cgi->param('milestone');
    my $bid   = $cgi->param('build_id');
    
    ThrowUserError('testopia-missing-required-field', {'field' => 'build name'}) unless $cname;
    
    my $build = Bugzilla::Testopia::Build->new($bid);
    
    trick_taint($cname);
    trick_taint($desc);
    trick_taint($milestone);
    validate_selection($milestone, 'value', 'milestones');
    
    my $orig_id = $build->check_name($cname);
    
    ThrowUserError('testopia-name-not-unique', 
                  {'object' => 'Build', 
                   'name' => $cname}) if ($orig_id && $orig_id != $bid);
    
    $build->update($cname, $desc, $milestone, $cgi->param('isactive') ? 1 : 0);
    $vars->{'tr_message'} = "Build successfully updated";
    display();
}

elsif ($action eq 'hide' || $action eq 'unhide'){
    my $bid   = $cgi->param('build_id');
    my $build = Bugzilla::Testopia::Build->new($bid);
    $build->toggle_hidden;
    display();
}

########################
### View plan Builds ###
########################
else {
    display();
}

###################
### Helper Subs ###
###################

sub display{
    $template->process("testopia/build/list.html.tmpl", $vars) 
      || ThrowTemplateError($template->error());    
}
