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

use lib qw(..);

use Getopt::Long;
use Litmus::Config;
use DBI ();

use Data::Dumper;
use diagnostics;

use vars qw(
	    $litmus_dbh
            $testgroup_id
	    );

END {
  if ($litmus_dbh) { 
    $litmus_dbh->disconnect; 
  }
}

$litmus_dbh = &connect_litmus() or die;

GetOptions('testgroup_id=i' => \$testgroup_id);

if (!$testgroup_id) {
  die &usage;
}

my ($sql,$sth);

$sql="INSERT INTO test_groups (product_id,name,testrunner_plan_id,enabled) SELECT product_id,name,testrunner_plan_id,enabled FROM test_groups WHERE testgroup_id=?";

print $sql,"\n";

my $rv;
$rv = $litmus_dbh->do($sql,undef,$testgroup_id);
if ($rv<=0) {
  die "Testgroup insert failed: $!";
}

$sql="SELECT MAX(testgroup_id) FROM test_groups";
$sth = $litmus_dbh->prepare($sql);
$sth->execute();
my ($new_testgroup_id) = $sth->fetchrow_array;
$sth->finish;

if (!$new_testgroup_id) {
  die "Unable to lookup new testgroup_id!";
}
print "New testgroup id: $new_testgroup_id\n";

$sql="SELECT subgroup_id,testgroup_id,name,sort_order,testrunner_group_id,enabled FROM subgroups WHERE testgroup_id=?";
$sth = $litmus_dbh->prepare($sql);
$sth->execute($testgroup_id);
my @subgroups;
while (my $hashref = $sth->fetchrow_hashref) {
  push @subgroups, $hashref;
}
$sth->finish;

foreach my $subgroup (@subgroups) {
  $rv = $litmus_dbh->do("INSERT INTO subgroups (testgroup_id,name,sort_order,testrunner_group_id,enabled) VALUES (?,?,?,?,?)",
                        undef,
                        $new_testgroup_id,
                        $subgroup->{'name'},
                        $subgroup->{'sort_order'},
                        $subgroup->{'testrunner_group_id'},
                        $subgroup->{'enabled'},
                       );                  
  $sql="SELECT MAX(subgroup_id) FROM subgroups";
  $sth = $litmus_dbh->prepare($sql);
  $sth->execute();
  my ($new_subgroup_id) = $sth->fetchrow_array;
  $sth->finish;
  $rv = $litmus_dbh->do("INSERT INTO tests (subgroup_id,summary,details,community_enabled,format_id,regression_bug_id,steps,expected_results,sort_order,author_id,creation_date,last_updated,version,testrunner_case_id,testrunner_case_version,enabled) SELECT $new_subgroup_id,summary,details,community_enabled,format_id,regression_bug_id,steps,expected_results,sort_order,author_id,creation_date,last_updated,version,testrunner_case_id,testrunner_case_version,enabled FROM tests WHERE subgroup_id=?",
                        undef,
                        $subgroup->{'subgroup_id'}
                       );
}

exit;

#########################################################################
sub usage() {
    print "Usage: ./clone_testgroup.pl --testgroup_id=#\n\n";
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







