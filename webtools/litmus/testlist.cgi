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

use CGI;
use Time::Piece::MySQL;

my $c = new CGI; 

print $c->header();

# takes either param tests, a comma separated list of test ids to display, 
# or if no param is given (for now), display a list of all failing tests 
# (very slowly)

my @testlist;

my $tests = $c->param("tests");
if ($tests) {
	my @tests = split("," => $tests);
	foreach my $curtest (@tests) {
		my $test = Litmus::DB::Test->retrieve($curtest);
		push(@testlist, $test);
	}
} else {
	Litmus::DB::Test->set_sql('failing' => qq {
		SELECT tests.test_id
		FROM tests, test_results
		WHERE
			test_results.result_id != 1 AND
			test_results.test_id = tests.test_id 
		GROUP BY tests.test_id
			
	});
	@testlist = Litmus::DB::Test->search_failing();
}

my $vars = {
    testlist => \@testlist,
};

Litmus->template()->process("list/list.html.tmpl", $vars) || 
        internalError(Litmus->template()->error());    
