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
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Util;
use Bugzilla::Testopia::Util;

use vars qw($template $vars);

Bugzilla->login();
print Bugzilla->cgi->header();
   
my $dbh = Bugzilla->dbh;
my $cgi = Bugzilla->cgi;
my $template = Bugzilla->template;
my $plan_id = trim(Bugzilla->cgi->param('plan_id') || '');

unless ($plan_id){
  $vars->{'form_action'} = 'tr_plan_reports.cgi';
  $template->process("testopia/plan/choose.html.tmpl", $vars) 
      || ThrowTemplateError($template->error());
  exit;
}
validate_test_id($plan_id, 'plan');
push @{$::vars->{'style_urls'}}, 'testopia/css/default.css';

my $action = $cgi->param('action') || '';
my $plan = Bugzilla::Testopia::TestPlan->new($plan_id);
my $report = {};
my %buildseen;
foreach my $case (@{$plan->test_cases}){
    foreach my $cr (@{$case->caseruns}){
        $buildseen{$cr->build->id} = $cr->build->name;
        $report->{$case->id}->{$cr->build->id} = $cr;
    }
}
$report->{'builds'} = \%buildseen;
$vars->{'report'} = $report;
$vars->{'plan'} = $plan;
$template->process("testopia/reports/build-coverage.html.tmpl", $vars)
   || ThrowTemplateError($template->error());

