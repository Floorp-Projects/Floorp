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

use Data::Dumper;
use Getopt::Long;

use lib qw(..);

use Litmus;
use Litmus::DBI;

use vars qw(
            $dbh
	    $sth
	    $sql
            );

END {
  if ($dbh) { $dbh->disconnect; }
}

my $orphans_only = 0;
my $help = 0;
my $skip_schema_update = 0;
GetOptions('orphans-only' => \$orphans_only,
           'skip-schema-update' => \$skip_schema_update,
           'help|?' => \$help);

if ($help) {
  &usage;
  exit;
}

$dbh = Litmus::DBI->db_Main;
if (!$dbh) {
  die "Unable to connect to database!";
}

if (!$orphans_only) {

#########################################################################
# Check to see whether any testgroups currently belong to more than one 
# branch. We can't handle this case programmatically.
#
# While we're here, collect the branch data for testgroups so we _can_
# proceed if possible.
#########################################################################
  print "Making sure testgroups belong to a single branch...";
  $sql = "SELECT COUNT(testgroup_id),testgroup_id,branch_id FROM testgroup_branches GROUP BY testgroup_id";
  $sth = $dbh->prepare($sql);
  my %testgroup_branches;
  my $num_branches = 0;
  my $testgroup_id = 0;
  my $branch_id = 0;
  $sth->execute();
  while (($num_branches,$testgroup_id,$branch_id) = $sth->fetchrow_array) {
    if ($num_branches>1) {
      last;
    }
    $testgroup_branches{$testgroup_id} = $branch_id;
  }
  $sth->finish;
  
  if ($num_branches and 
      $num_branches>1) {
    die "\nTestgroup ID# $testgroup_id is associated with more than one branch.
Script cannot proceed until this is resolved by hand.\n";
  }
  
  print "Done\n";

#########################################################################
# Make the necessary changes to the db schema
#########################################################################
  if (!$skip_schema_update) {
    print "Updating db schema...";
    $sql = "DROP TABLE testgroup_branches";
    if (!$dbh->do($sql)) {
      die "\nFailed to drop testgroup_branches table.\n";
    }
    
    $sql = "ALTER TABLE testgroups ADD COLUMN branch_id smallint(6) NOT NULL default '0'";
    if (!$dbh->do($sql)) {
      die "\nFailed to add branch_id column to testgroups table.\n";
    }
    $sql = "ALTER TABLE testgroups ADD KEY branch_id (branch_id)";
    if (!$dbh->do($sql)) {
      die "\nFailed to add branch_id key to testgroups table.\n";
    }
    
    $sql = "ALTER TABLE subgroups ADD COLUMN branch_id smallint(6) NOT NULL default '0'";
    if (!$dbh->do($sql)) {
      die "\nFailed to add branch_id column to subgroups table.\n";
    }
    $sql = "ALTER TABLE subgroups ADD KEY branch_id (branch_id)";
    if (!$dbh->do($sql)) {
      die "\nFailed to add branch_id key to subgroups table.\n";
    }
    
    $sql = "ALTER TABLE testcases ADD COLUMN branch_id smallint(6) NOT NULL default '0'";
    if (!$dbh->do($sql)) {
      die "\nFailed to add branch_id column to testcases table.\n";
    }
    $sql = "ALTER TABLE testcases ADD KEY branch_id (branch_id)";
    if (!$dbh->do($sql)) {
      die "\nFailed to add branch_id key to testcases table.\n";
    }
    
    print "Done\n";

  }

#########################################################################
# Update the testgroups table with branch info.
#########################################################################
  print "Updating testgroups with branch info...";
  $sql = 'UPDATE testgroups set branch_id=? where testgroup_id=?';
  foreach my $testgroup_id (keys %testgroup_branches) {
    if (!$dbh->do($sql,
                  undef,
                  $testgroup_branches{$testgroup_id},
                  $testgroup_id)) {
      warn "\nFailed to update testgroup ID# $testgroup_id\n";
    }
  }
  $sth->finish;
  
  print "Done\n";

#########################################################################
# Get subgroups that belong to testgroups. We can't match subgroups to 
# branches unless they belong to a testgroup, so we'll just print out a 
# list of orphaned subgroups at the end.
#########################################################################
  print "Finding subgroups...";
  
  my %subgroup_testgroups;
  $sql = "SELECT subgroup_id, testgroup_id FROM subgroup_testgroups";
  $sth = $dbh->prepare($sql);
  $sth->execute();
  while (my ($subgroup_id,$testgroup_id) = $sth->fetchrow_array) {
    $subgroup_testgroups{$subgroup_id} = $testgroup_id;
  }
  $sth->finish;
  
  print "Done\n";
  
#########################################################################
# Determine subgroup branches based on testgroups
#########################################################################
  print "Matching subgroups to branches...";
  $sql = 'UPDATE subgroups set branch_id=? where subgroup_id=?';
  foreach my $subgroup_id (keys %subgroup_testgroups) {
    if ($testgroup_branches{$subgroup_testgroups{$subgroup_id}}) {
      if (!$dbh->do($sql,
                    undef,
                    $testgroup_branches{$subgroup_testgroups{$subgroup_id}},
                    $subgroup_id)) {
        warn "\nFailed to update subgroup ID# $subgroup_id\n";
      }
    }
  }
  $sth->finish;
  
  print "Done\n";

#########################################################################
# Get testcases that belong to subgroups/testgroups. We can't match 
# testcases to branches unless they belong to a subgroup and testgroup, 
# so we'll just print out alist of orphaned testcases at the end.
#########################################################################
  print "Finding testcases...";
  
  my %testcase_subgroups;
  $sql = "SELECT testcase_id, subgroup_id FROM testcase_subgroups";
  $sth = $dbh->prepare($sql);
  $sth->execute();
  while (my ($testcase_id,$subgroup_id) = $sth->fetchrow_array) {
    $testcase_subgroups{$testcase_id} = $subgroup_id;
  }
  $sth->finish;
  
  print "Done\n";

#########################################################################
# Determine subgroup branches based on testgroups
#########################################################################
  print "Matching testcases to branches...";
  $sql = 'UPDATE testcases set branch_id=? where testcase_id=?';
  foreach my $testcase_id (keys %testcase_subgroups) {
    if ($subgroup_testgroups{$testcase_subgroups{$testcase_id}} and
        $testgroup_branches{$subgroup_testgroups{$testcase_subgroups{$testcase_id}}}) {
      if (!$dbh->do($sql,
                    undef,
                    $testgroup_branches{$subgroup_testgroups{$testcase_subgroups{$testcase_id}}},
                    $testcase_id)) {
        warn "\nFailed to update testcase ID# $testcase_id\n";
      }
    }
  }
  $sth->finish;
  
  print "Done\n";
  
}

#########################################################################
# Generate lists of orphaned (i.e. no branch) testgroups, subgroups, and 
# testcases.
#########################################################################
print "Checking for orphaned testgroups...\n";
$sql = "SELECT testgroup_id,name FROM testgroups WHERE branch_id=0 ORDER BY testgroup_id ASC";
$sth = $dbh->prepare($sql);
$sth->execute();
my $num_orphans=0;
while (my ($testgroup_id,$name) = $sth->fetchrow_array) {
  print "$testgroup_id: $name\n";
  $num_orphans++;
}
$sth->finish;

print "\n# of orphaned testgroups found: $num_orphans\n\n";

print "Checking for orphaned subgroups...\n";
$sql = "SELECT subgroup_id,name FROM subgroups WHERE branch_id=0 ORDER BY subgroup_id ASC";
$sth = $dbh->prepare($sql);
$sth->execute();
$num_orphans=0;
while (my ($subgroup_id,$name) = $sth->fetchrow_array) {
  print "$subgroup_id: $name\n";
  $num_orphans++;
}
$sth->finish;

print "\n# of orphaned subgroups found: $num_orphans\n\n";

print "Checking for orphaned testcases...\n";
$sql = "SELECT testcase_id,summary FROM testcases WHERE branch_id=0 ORDER BY testcase_id ASC";
$sth = $dbh->prepare($sql);
$sth->execute();
$num_orphans=0;
while (my ($testcase_id,$summary) = $sth->fetchrow_array) {
  print "$testcase_id: $summary\n";
  $num_orphans++;
}
$sth->finish;

print "\n# of orphaned testcases found: $num_orphans\n\n";

print "NOTE: if there are many orphaned testcases above, it is usually easier
to try updating any orphaned testgroups or subgroups listed above, and then 
re-run this updating script with the --skip-schema-update option. This may 
save you a lot of unnecessary work.\n\n";

#########################################################################
sub usage() {
  print <<EOS
Usage: ./update_branch_relationships.pl [--help]
                                        [--orphans-only]
                                        [--skip-schema-update]
EOS

}




