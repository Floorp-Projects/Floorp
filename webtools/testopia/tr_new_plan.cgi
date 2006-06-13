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
use Bugzilla::Testopia::TestPlan;

require "globals.pl";

use vars qw($template $vars);

Bugzilla->login(LOGIN_REQUIRED);
ThrowUserError("testopia-create-denied", {'object' => 'Test Plan'}) unless UserInGroup('managetestplans');

print Bugzilla->cgi->header();
   
my $dbh = Bugzilla->dbh;
my $cgi = Bugzilla->cgi;

push @{$::vars->{'style_urls'}}, 'testopia/css/default.css';

my $action = $cgi->param('action');
my $product = $cgi->param('prod_id');

detaint_natural($product);

    
if ($action eq 'Add'){
    my $name = $cgi->param('plan_name');
    my $prodver = $cgi->param('prod_version');
    my $type = $cgi->param('type');
    my $text = $cgi->param("plandoc");
    validate_selection($product, 'id', 'products');
    ThrowUserError('testopia-missing-required-field', {'field' => 'name'}) if $name  eq '';

    # All inserts are done with placeholders so this is OK
    trick_taint($name);
    trick_taint($prodver);
    trick_taint($text);
    detaint_natural($type);
    
    validate_selection($type, 'type_id', 'test_plan_types');
    #TODO: 2.22 use Bugzilla::Product
    validate_selection($prodver, 'value', 'versions');
    
    my $plan = Bugzilla::Testopia::TestPlan->new({
            'name'       => $name || '',
            'product_id' => $product,
            'default_product_version' => $prodver,
            'type_id'    => $type,
            'text'       => $text,
            'author_id'  => Bugzilla->user->id,
            'editor_id'  => Bugzilla->user->id,
    });
    my $plan_id = $plan->store;
    $vars->{'case_table'} = undef;
    $vars->{'case_table'} = undef;
    $vars->{'action'} = "Commit";
    $vars->{'form_action'} = "tr_show_plan.cgi";
    $vars->{'plan'} = Bugzilla::Testopia::TestPlan->new($plan_id);
    $template->process("testopia/plan/show.html.tmpl", $vars) ||
        ThrowTemplateError($template->error());
    
}
####################
### Ajax Actions ###
####################
elsif ($action eq 'getversions'){
    my $plan = Bugzilla::Testopia::TestPlan->new(
        {'plan_id'    => 0, 
         'product_id' => $product});
    my $versions = $plan->get_product_versions($product);
    my $ret;
    foreach my $v (@{$versions}){
        $ret .= $v->{'id'} . "|";
    }
    chop($ret);
    print $ret;
}
####################
### Display Form ###
####################
else {
    $vars->{'action'} = "Add";
    $vars->{'form_action'} = "tr_new_plan.cgi";
    $vars->{'plan'} = Bugzilla::Testopia::TestPlan->new(
        {'plan_id'    => 0, 
         'product_id' => $product});
    $template->process("testopia/plan/add.html.tmpl", $vars) ||
        ThrowTemplateError($template->error());
}
