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

use Litmus;
use Litmus::Auth;
use Litmus::Error;
use Litmus::DBI;

use CGI;
use Time::Piece::MySQL;

Litmus->init();
my $c = Litmus->cgi();
print $c->header();

my $template = "reporting/aggregate_results.tmpl";
my $title = "Test Results Submitted by Testgroup";

my $results;
my $dbh = Litmus::DBI->db_Main();
my $sql = "SELECT tg.name AS testgroup_name,pr.name AS product_name,b.name AS branch_name,count(tr.testresult_id) AS num_results 
  FROM testgroups tg, products pr, branches b, testgroup_branches tgb, subgroup_testgroups sgtg, testcase_subgroups tcsg, testcases tc, test_results tr 
  WHERE tg.testgroup_id=tgb.testgroup_id AND tgb.branch_id=b.branch_id AND tg.product_id=pr.product_id AND tg.enabled=1 AND tg.testgroup_id=sgtg.testgroup_id AND sgtg.subgroup_id=tcsg.subgroup_id AND tcsg.testcase_id=tc.testcase_id AND tc.testcase_id=tr.testcase_id AND tc.enabled=1 AND tr.branch_id=tgb.branch_id AND tc.product_id=tg.product_id 
  GROUP BY tg.name,pr.name,b.name 
  ORDER BY pr.name, b.name";
my $sth = $dbh->prepare($sql);
$sth->execute();
while (my $data = $sth->fetchrow_hashref) {
  push @$results, $data;
}
$sth->finish();

my $vars = {
            title => $title,
            status => $c->param('status'),
           };

# Only include results if we have them.
if ($results and scalar @$results > 0) {
    $vars->{results} = $results;
}

$vars->{"defaultemail"} = Litmus::Auth::getCookie();

Litmus->template()->process($template, $vars) ||
  internalError(Litmus->template()->error());

exit 0;






