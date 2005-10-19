#!/usr/bin/perl -w
# -*- mode: cperl; c-basic-offset: 8; indent-tabs-mode: nil; -*-

# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Litmus.
#
# The Initial Developer of the Original Code is
# the Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Chris Cooper <ccooper@deadsquid.com>
#   Zach Lipton <zach@zachlipton.com>
#
# ***** END LICENSE BLOCK *****

use strict;
$|++;

use Litmus;
use Litmus::Auth;
use Litmus::Error;
use Litmus::DB::Testresult;
use Litmus::FormWidget;
use diagnostics;

use CGI;
use Time::Piece::MySQL;


my $c = new CGI;
print $c->header();

my $results;
if ($c->param and $c->param('status')) {
  if ($c->param('status') =~ /pass/i or
      $c->param('status') =~ /fail/i or
      $c->param('status') =~ /unclear/i) {
    $results = Litmus::DB::Testresult->getCommonResults($c->param('status'));
  } else {
    internalError("You must provide a valid status type: pass|fail|unclear");
    exit 1;
  }                                                      
} else {
  internalError("You must provide a status type: pass|fail|unclear");
  exit 1;
}

my $title;
if ($c->param('status') eq 'pass') {
  $title = "Most Commonly Passed Testcases";
} elsif ($c->param('status') eq 'fail') {
  $title = "Most Common Failures";
} elsif ($c->param('status') eq 'unclear') {
  $title = "Testcases Most Frequently Marked As Unclear";
} 

my $vars = {
            title => $title,
            status => $c->param('status'),
           };

# Only include results if we have them.
if ($results and scalar @$results > 0) {
    $vars->{results} = $results;
}

$vars->{"defaultemail"} = Litmus::Auth::getCookie();

Litmus->template()->process("reporting/common_results.tmpl", $vars) ||
  internalError(Litmus->template()->error());

exit 0;






