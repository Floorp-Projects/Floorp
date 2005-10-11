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

use Time::HiRes qw( gettimeofday tv_interval );
my $t0 = [gettimeofday];

use Litmus;
use Litmus::Auth;
use Litmus::Error;
use Litmus::DB::Testresult;
use Litmus::FormWidget;
use diagnostics;

use CGI;
use Time::Piece::MySQL;

my $results = Litmus::DB::Testresult->getCommonFailures;

my $c = new CGI;
print $c->header();

my $vars = {
            title => 'Most Common Failures',
           };

# Only include results if we have them.
if ($results and scalar @$results > 0) {
    $vars->{results} = $results;
}

$vars->{"defaultemail"} = Litmus::Auth::getCookie();

Litmus->template()->process("reporting/common_failures.tmpl", $vars) ||
  internalError(Litmus->template()->error());

my $elapsed = tv_interval ( $t0 );
printf  "<p>Page took %f seconds to load.</p>", $elapsed;

exit 0;






