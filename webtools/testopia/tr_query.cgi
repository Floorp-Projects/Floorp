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
# Contributor(s): Maciej Maczynski <macmac@xdsnet.pl>
#                 Ed Fuentetaja <efuentetaja@acm.org>
#                 Greg Hendricks <ghendricks@novell.com>

use strict;
use lib ".";

use Bugzilla;
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::TestPlan;
use Bugzilla::Testopia::TestRun;

use vars qw($vars $template);
require "globals.pl";
my $dbh = Bugzilla->dbh;
my $cgi = Bugzilla->cgi;
my $template = Bugzilla->template;

push @{$::vars->{'style_urls'}}, 'testopia/css/default.css';

Bugzilla->login();
print $cgi->header;

my $action = $cgi->param('action');

if ($action eq 'getversions'){
    my @prod_ids = $cgi->param('prod_ids');
    my $plan = Bugzilla::Testopia::TestPlan->new({'plan_id'    => 0});
    my @versions;
    foreach my $p (@prod_ids){
        detaint_natural($p);
        push @versions, $plan->get_product_versions($p);
    }
    # Weed out duplicates
    my %seen;
    foreach my $v (@versions){
        $seen{$v->{'id'}} = 1;
    }
    my $ret;
    foreach my $v (keys %seen){
        $ret .= $v . "|";
    }
    chop($ret);
    print $ret;
}
else{
    #TODO: Support default query
    my $tab = $cgi->param('current_tab') || '';
    if ($tab eq 'plan'){
        my $plan = Bugzilla::Testopia::TestPlan->new({ 'plan_id' => 0 });
        my @allversions;
        foreach my $p (@{$plan->get_available_products}){
            push @allversions, @{$plan->get_product_versions($p->{'id'})};
        }
        # weed out any repeats
        my %versions;
        foreach my $v (@allversions){
            $versions{$v->{'id'}} = 1;
        }
        @allversions = ();
        foreach my $k (keys %versions){
            push @allversions, { 'id' => $k,  'name' => $k };
        }
        $vars->{'plan'} = $plan;
        $vars->{'title'} = "Search For Test Plans";
        $vars->{'product_versions'} = \@allversions;
    }
    elsif ($tab eq 'run'){
        my $run = Bugzilla::Testopia::TestRun->new({ 'run_id' => 0 });
        $vars->{'run'} = $run;
        # TODO Narrow build list by product
        $vars->{'title'} = "Search For Test Runs";
    }
    else { # show the case form
        $tab = 'case';
        my $case = Bugzilla::Testopia::TestCase->new({ 'case_id' => 0 });
        $vars->{'case'} = $case;
        $vars->{'title'} = "Search For Test Cases";
    }
    
    $vars->{'current_tab'} = $tab;
    $template->process("testopia/search/advanced.html.tmpl", $vars)
        || ThrowTemplateError($template->error()); 
}
    
