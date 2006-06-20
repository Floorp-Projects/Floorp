#!/usr/bin/perl -w
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
# The Original Code is Litmus.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Zach Lipton <zach@zachlipton.com>

use strict;

use Litmus;
use Litmus::Error;
use Litmus::DB::Product;
use Litmus::DB::TestcaseSubgroup;
use Litmus::Auth;
use Litmus::Utils;

use CGI;
use Time::Piece::MySQL;
use Date::Manip;

my $c = Litmus->cgi(); 

# for the moment, you must be an admin to enter tests:
Litmus::Auth::requireAdmin('enter_test.cgi');

# if we're here for the first time, display the enter testcase form, 
# otherwise, process the results: 

if (! $c->param('enteringTestcase')) {
	my $vars = {

	};

	print $c->header();
	Litmus->template()->process("enter/enter.html.tmpl", $vars) || 
        internalError(Litmus->template()->error()); 
} else {
	requireField('product', $c->param('product'));
	requireField('test group', $c->param('testgroup'));
	requireField('subgroup', $c->param('subgroup'));
	requireField('summary', $c->param('summary'));
	
	my $newtest = Litmus::DB::Testcase->create({
		product => $c->param('product'),
		summary => $c->param('summary'), 
		steps => $c->param('steps') ? $c->param('steps') : '',
		expected_results => $c->param('expectedResults') ? 
			$c->param('expectedResults') : '',
		author  => Litmus::Auth::getCurrentUser(),
		creation_date => &Date::Manip::UnixDate("now","%q"),
		version => 1,
	});
	
	my $newtsg = Litmus::DB::TestcaseSubgroup->create({
		test => $newtest,
		subgroup => $c->param('subgroup'),
	});
	
	my $vars = {
		test => $newtest,
	};
	
	print $c->header();
	Litmus->template()->process("enter/enterComplete.html.tmpl", $vars) || 
        internalError(Litmus->template()->error()); 
}
