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
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Chris Cooper <ccooper@deadsquid.com>
#   Zach Lipton <zach@zachlipton.com>
#
# ***** END LICENSE BLOCK *****

use strict;
$|++;

#use Time::HiRes qw( gettimeofday tv_interval );
#my $t0 = [gettimeofday];

use Litmus;
use Litmus::Auth;
use Litmus::Error;
use Litmus::DB::Testresult;
use Litmus::FormWidget;

use CGI;
use Time::Piece::MySQL;

use diagnostics;

my ($criteria,$results) = Litmus::DB::Testresult->getDefaultTestResults;

my $products = Litmus::FormWidget->getProducts();
my $platforms = Litmus::FormWidget->getUniquePlatforms();
my $test_groups = Litmus::FormWidget->getTestgroups();
my $result_statuses = Litmus::FormWidget->getResultStatuses;
my $branches = Litmus::FormWidget->getBranches();

my $c = Litmus->cgi();
print $c->header();

my $vars = {
            title => 'Main Page',
            products => $products,
            platforms => $platforms,
            test_groups => $test_groups,
            result_statuses => $result_statuses,
            branches => $branches,
            limit => $Litmus::DB::Testresult::_num_results_default,
           };

# Only include results if we have them.
if ($results and scalar @$results > 0) {
  $vars->{results} = $results;
}

my $user = Litmus::Auth::getCurrentUser();
if ($user) {
	$vars->{"defaultemail"} = $user;
	$vars->{"show_admin"} = $user->is_admin();
}

Litmus->template()->process("index.tmpl", $vars) ||
  internalError(Litmus->template()->error());

#my $elapsed = tv_interval ( $t0 );
#printf  "<div id='pageload'>Page took %f seconds to load.</div>", $elapsed;

exit 0;






