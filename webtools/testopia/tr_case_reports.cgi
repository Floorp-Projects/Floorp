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
use Bugzilla::Testopia::Constants;

use vars qw($template $vars);
my $template = Bugzilla->template;
my $cgi = Bugzilla->cgi;

Bugzilla->login();
print $cgi->header();
   
my $case_id = trim(Bugzilla->cgi->param('case_id') || '');

unless ($case_id){
  $vars->{'form_action'} = 'tr_case_reports.cgi';
  $template->process("testopia/case/choose.html.tmpl", $vars) 
      || ThrowTemplateError($template->error());
  exit;
}
validate_test_id($case_id, 'case');
push @{$::vars->{'style_urls'}}, 'testopia/css/default.css';

my $case = Bugzilla::Testopia::TestCase->new($case_id);

my $type = $cgi->param('type') || '';

if ($type eq 'status-breakdown'){
    
    my @data;
    my $caserun = Bugzilla::Testopia::TestCaseRun->new({});
    
    my @names;
    my @values;
    foreach my $status (@{$caserun->get_status_list}){
         push @names, $status->{'name'};
         push @values, $case->get_caserun_count($status->{'id'});
    }
    push @data, \@names;
    push @data, \@values;

    $vars->{'width'} = 200;
    $vars->{'height'} = 150;
    $vars->{'data'} = \@data;
    $vars->{'chart_title'} = 'Historic Status Breakdown';
    $vars->{'colors'} = (['#858aef', '#56e871', '#ed3f58', '#b8eae1', '#f1d9ab', '#e17a56']);
    
    $template->process("testopia/reports/pie.png.tmpl", $vars)
       || ThrowTemplateError($template->error());
}
