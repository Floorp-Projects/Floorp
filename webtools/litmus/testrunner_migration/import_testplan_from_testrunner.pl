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

use lib qw(..);

use Getopt::Long;
use Litmus::Config;
use DBI ();

use Data::Dumper;
use diagnostics;

use vars qw(
            $tr_dbh
            $litmus_dbh
            $plan_id
            $product_id
            $author_id
	    );

GetOptions (
            'plan_id=i' => \$plan_id,
            'product_id=i' => \$product_id,
            'author_id=i' => \$author_id,
           );

# Default to Firefox
if (!$product_id) {
  $product_id=1;
}

# Default to Coop
if (!$author_id) {
  $author_id=4;
}

if (!$plan_id or $plan_id eq '') {
    die &usage;
}

END {
  if ($tr_dbh) { $tr_dbh->disconnect; }
  if ($litmus_dbh) { $litmus_dbh->disconnect; }
}

$litmus_dbh = &connect_litmus() or die;
$tr_dbh = &connect_testrunner() or die;

my ($sql,$sth);

# Get test plan info from TestRunner
$sql="SELECT name FROM test_plans WHERE plan_id=?";
$sth = $tr_dbh->prepare($sql);
$sth->execute($plan_id);
my ($new_testgroup_name) = $sth->fetchrow_array;
$sth->finish;

if (!$new_testgroup_name or $new_testgroup_name eq '') {
  die "Unable to lookup test plan name for id: $plan_id";
}

# Get all subgroups belonging to the test plan
$sql="select group_id, plan_id, name, description from test_case_groups where plan_id=?";

$sth = $tr_dbh->prepare($sql);
$sth->execute($plan_id);
my @result;
#print Dumper  $sth->{NAME};
my $i=1;
my @subgroups;
while (my $result = $sth->fetchrow_hashref) {
  $result->{'name'} =~ s/^\d+\. //g;
  push @subgroups, $result;
}
$sth->finish;

# Get most recent testcase info for each subgroup.
$sql = "select max(tct.case_version),tct.summary,tct.action,tct.effect,tc.case_id from test_cases tc, test_cases_texts tct where tc.group_id=? and tc.case_id=tct.case_id GROUP BY tct.case_id"; 
$sth = $tr_dbh->prepare($sql);
my $tests;
foreach my $subgroup (@subgroups) {
  $sth->execute($subgroup->{'group_id'});
  while (my $result = $sth->fetchrow_hashref) {
    push @{$tests->{$subgroup->{'group_id'}}}, $result;
  }
}
$sth->finish;

# Create our top-level test group
my $rv;
$rv = $litmus_dbh->do("INSERT INTO test_groups (product_id,name,testrunner_plan_id,enabled) VALUES (?,?,?,1)",
                      undef,
                      $product_id,
                      $new_testgroup_name,
                      $plan_id
                     );

if ($rv<=0) {
  die "Failed to insert new testgroup.";
}

$sql = "SELECT MAX(testgroup_id) FROM test_groups";
$sth = $litmus_dbh->prepare($sql);
$sth->execute();
my ($new_testgroup_id) =  $sth->fetchrow_array;
$sth->finish;

if (!$new_testgroup_id) {
  die "Unable to lookup new testgroup ID";
}

my $subgroup_sql = "INSERT INTO subgroups (testgroup_id,name,sort_order,testrunner_group_id) VALUES (?,?,?,?)";
my $new_id_sql = "SELECT MAX(subgroup_id) FROM subgroups";
my $test_sql = "INSERT INTO tests (subgroup_id, summary, details, community_enabled, steps, expected_results, sort_order, author_id, creation_date, last_updated, testrunner_case_id, testrunner_case_version, enabled) VALUES (?,?,?,1,?,?,?,?,NOW(),NOW(),?,1,1)";
my $group_sort_order=1;
foreach my $subgroup (@subgroups) {
  $litmus_dbh->do($subgroup_sql,
                  undef, 
                  $new_testgroup_id, 
                  $subgroup->{'name'}, 
                  $group_sort_order,
                  $subgroup->{'group_id'},            
                 );

  $sth = $litmus_dbh->prepare($new_id_sql);
  $sth->execute();
  ($subgroup->{'subgroup_id'}) =  $sth->fetchrow_array;
  $sth->finish;
  $group_sort_order++;

  $sth = $litmus_dbh->prepare($test_sql);
  my $test_sort_order=1;
  foreach my $test (@{$tests->{$subgroup->{'group_id'}}}) {
    $litmus_dbh->do($test_sql, 
                    undef,
                    $subgroup->{'subgroup_id'},
                    $test->{'summary'},
                    $test->{'summary'},
                    $test->{'action'},
                    $test->{'effect'},
                    $test_sort_order,
                    $author_id,
                    $test->{'case_id'}
                   );
    $test_sort_order++;
  }
}

exit;

#########################################################################
sub usage() {
    print "Usage: ./import_testplan_from_testrunner.pl --plan_id=#\n";
    print "                                            [--product_id=#]\n";
    print "                                            [--author_id=#]\n\n";
}

#########################################################################
sub connect_testrunner() {
  my $dsn = "dbi:mysql:" . $Litmus::Config::tr_name . 
    ":" . $Litmus::Config::tr_host;
  my $dbh = DBI->connect($dsn,
                         $Litmus::Config::tr_user,
                         $Litmus::Config::tr_pass) 
    || die "Could not connect to mysql database $Litmus::Config::tr_name";
  
  return $dbh;
}

#########################################################################
sub connect_litmus() {
  my $dsn = "dbi:mysql:" . $Litmus::Config::db_name . 
    ":" . $Litmus::Config::db_host;
  my $dbh = DBI->connect($dsn,
                         $Litmus::Config::db_user,
                         $Litmus::Config::db_pass)
    || die "Could not connect to mysql database $Litmus::Config::db_name";
  
  return $dbh;
}








