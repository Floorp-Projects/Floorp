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
use JSON;

use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::TestPlan;
use Bugzilla::Testopia::Product;

require "globals.pl";

use vars qw($template $vars);
my $template = Bugzilla->template;

Bugzilla->login(LOGIN_REQUIRED);
ThrowUserError("testopia-create-denied", {'object' => 'Test Plan'}) unless UserInGroup('managetestplans');

print Bugzilla->cgi->header();
   
my $dbh = Bugzilla->dbh;
my $cgi = Bugzilla->cgi;

push @{$::vars->{'style_urls'}}, 'testopia/css/default.css';

my $action = $cgi->param('action');
my $product = $cgi->param('product_id');

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
    });
    my $plan_id = $plan->store;
    my @dojo_search;
    push @dojo_search, "plandoc","newtag","tagTable";
    $vars->{'dojo_search'} = objToJson(\@dojo_search);
    $vars->{'case_table'} = undef;
    $vars->{'case_table'} = undef;
    $vars->{'action'} = "Commit";
    $vars->{'form_action'} = "tr_show_plan.cgi";
    $vars->{'plan'} = Bugzilla::Testopia::TestPlan->new($plan_id);
    $vars->{'tr_message'} = "Test Plan: \"". $plan->name ."\" created successfully.";
    $vars->{'backlink'} = $vars->{'plan'};
    $template->process("testopia/plan/show.html.tmpl", $vars) ||
        ThrowTemplateError($template->error());
    
}
####################
### Ajax Actions ###
####################
elsif ($action eq 'getversions'){
    print $cgi->header;
    my $plan = Bugzilla::Testopia::TestPlan->new({});
    my $prod_id = $cgi->param("product_id");
    my @versions;
    if ($prod_id == -1){
        # For update multiple from tr_list_plans
        push @versions, {'id' => "--Do Not Change--", 'name' => "--Do Not Change--"};
    }
    else{
        detaint_natural($prod_id);
        my $prod = $plan->lookup_product($prod_id);
        unless (Bugzilla->user->can_see_product($prod)){
            print '{ERROR:"You do not have permission to view this product"}';
            exit;
        }
        my $product = Bugzilla::Testopia::Product->new($prod_id);
        @versions = @{$product->versions};
    }
    my $json = new JSON;
    $json->autoconv(0);
    print $json->objToJson(\@versions);
}
# For use in new_case and show_case since new_plan does not require an id
elsif ($action eq 'getcomps'){
    Bugzilla->login;
    my $plan = Bugzilla::Testopia::TestPlan->new({});
    my $product_id = $cgi->param('product_id');

    detaint_natural($product_id);
    my $prod = $plan->lookup_product($product_id);
    unless (Bugzilla->user->can_see_product($prod)){
        print '{ERROR:"You do not have permission to view this product"}';
        exit;
    }
    my $product = Bugzilla::Testopia::Product->new($product_id);
    
    my @comps;
    foreach my $c (@{$product->components}){
        push @comps, {'id' => $c->id, 'name' => $c->name, 'qa_contact' => $c->default_qa_contact->login};
    }
    my $json = new JSON;
    print $json->objToJson(\@comps);
    exit;
}
elsif ($action eq 'getenvs'){
    Bugzilla->login;
    my $product_id = $cgi->param('product_id');

    detaint_natural($product_id);
    my $product = Bugzilla::Testopia::Product->new($product_id);
    
    my @envs;
    foreach my $e (@{$product->environments}){
        push @envs, {'id' => $e->id, 'name' => $e->name};
    }
    my $json = new JSON;
    print $json->objToJson(\@envs);
    exit;
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
