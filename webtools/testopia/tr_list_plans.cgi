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
# Contributor(s):  Greg Hendricks <ghendricks@novell.com>

use strict;
use lib ".";

use Bugzilla;
use Bugzilla::Config;
use Bugzilla::Error;
use Bugzilla::Constants;
use Bugzilla::Util;
use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::Search;
use Bugzilla::Testopia::Table;

use vars qw($vars $template);

require "globals.pl";
my $dbh = Bugzilla->dbh;
my $cgi = Bugzilla->cgi;
my $template = Bugzilla->template;

Bugzilla->login();
print $cgi->header;

my $action = $cgi->param('action') || '';
if ($action eq 'Commit'){
    Bugzilla->login(LOGIN_REQUIRED);
    # Get the list of checked items. This way we don't have to cycle through 
    # every test case, only the ones that are checked.
    my $reg = qr/p_([\d]+)/;
    my @plans;
    foreach my $p ($cgi->param()){
        push @plans, Bugzilla::Testopia::TestPlan->new($1) if $p =~ $reg;
    }
    ThrowUserError('testopia-none-selected', {'object' => 'plan'}) if (scalar @plans < 1);
    foreach my $plan (@plans){
        ThowUserError('insufficient-perms') unless $plan->canedit;
        my $plan_type = $cgi->param('plan_type')    == -1 ? $plan->type_id : $cgi->param('plan_type');
        my $product   = $cgi->param('product_id')   == -1 ? $plan->product_id : $cgi->param('product_id');
        my $prodver   = $cgi->param('prod_version') == -1 ? $plan->product_version : $cgi->param('prod_version');

        # We use placeholders so trick_taint is ok.
        trick_taint($prodver) if $prodver;
        
        detaint_natural($product);
        detaint_natural($plan_type);
        
        my %newvalues = ( 
            'type_id'    => $plan_type,
            'product_id' => $product,
            'default_product_version' => $prodver,
        );
        $plan->update(\%newvalues);
        $plan->toggle_archive if $cgi->param('togglearch');
        if ($cgi->param('addtags')){
            foreach my $name (split(/[\s,]+/, $cgi->param('addtags'))){
                trick_taint($name);
                my $tag = Bugzilla::Testopia::TestTag->new({'tag_name' => $name});
                my $tag_id = $tag->store;
                $plan->add_tag($tag_id);
            }
        }     
    }
    my $plan = Bugzilla::Testopia::TestPlan->new({ 'plan_id' => 0 });
    $vars->{'plan'} = $plan;
    $vars->{'title'} = "Update Susccessful";
    $vars->{'tr_message'} = scalar @plans . " Test Plan(s) Updated";
    $vars->{'current_tab'} = 'plan';
    $template->process("testopia/search/advanced.html.tmpl", $vars)
        || ThrowTemplateError($template->error()); 
    exit;

}
else {
    $cgi->param('current_tab', 'plan');
    my $search = Bugzilla::Testopia::Search->new($cgi);
    my $table = Bugzilla::Testopia::Table->new('plan', 'tr_list_plans.cgi', $cgi, undef, $search->query);    

    my $p = Bugzilla::Testopia::TestPlan->new({'plan_id' => 0 });
    my $product_list   = $p->get_available_products;
    my $prodver_list   = $p->get_product_versions;
    my $type_list      = $p->get_plan_types;
    
    unshift @{$product_list}, {'id' => -1, 'name' => "--Do Not Change--"};
    unshift @{$prodver_list}, {'id' => -1, 'name' => "--Do Not Change--"};
    unshift @{$type_list},    {'id' => -1, 'name' => "--Do Not Change--"};
    $vars->{'product_list'} = $product_list;
    $vars->{'prodver_list'} = $prodver_list;
    $vars->{'type_list'} = $type_list;
    
    $vars->{'fullwidth'} = 1; #novellonly
    $vars->{'dotweak'} = UserInGroup('managetestplans');
    $vars->{'table'} = $table;
    $template->process("testopia/plan/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error()); 

}
