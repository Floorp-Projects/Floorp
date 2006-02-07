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
	    $tr_dbh
	    $litmus_dbh
	    );

END {
  if ($tr_dbh) { $tr_dbh->disconnect; }
  if ($litmus_dbh) { $litmus_dbh->disconnect; }
}

$litmus_dbh = &connect_litmus() or die;
$tr_dbh = &connect_testrunner() or die;

my ($sql,$sth);

$sql="SELECT name,testgroup_id,testrunner_plan_id FROM test_groups WHERE testrunner_plan_id is not NULL";

$sth = $litmus_dbh->prepare($sql);
$sth->execute();
my $testgroups;
while (my ($name,$tg_id,$tr_p_id) = $sth->fetchrow_array) {
  $testgroups->{$tg_id}->{'name'} = $name;
  $testgroups->{$tg_id}->{'tr_p_id'} = $tr_p_id;
}
$sth->finish;

my $testcases_updated = 0;
foreach my $id (keys %$testgroups) {
  # Get existing Litmus subgroups.
  $sql="SELECT name,subgroup_id,testrunner_group_id FROM subgroups WHERE testgroup_id=?";
  $sth = $litmus_dbh->prepare($sql);
  $sth->execute($id);
  my $subgroups;
  while (my ($name,$s_id,$tr_g_id) = $sth->fetchrow_array) {
    if (!$tr_g_id) {
      print "# No TR info for subgroup ID: $s_id, $name\n";
      next;
    }
    $subgroups->{$tr_g_id}->{'name'} = $name;
    $subgroups->{$tr_g_id}->{'s_id'} = $s_id;
  }
  $sth->finish;
  
  # Get all subgroups from Testrunner.
  $sql="SELECT name,group_id FROM test_case_groups WHERE plan_id=?";
  $sth = $tr_dbh->prepare($sql);
  $sth->execute($testgroups->{$id}->{'tr_p_id'});
  my $tr_subgroups;
  while (my ($name,$s_id) = $sth->fetchrow_array) {
    $tr_subgroups->{$s_id}->{'name'} = $name;
  }
  $sth->finish;
  
  # Check for missing subgroups in Litmus.
  foreach my $s_id (keys %$tr_subgroups) {
    if (exists $subgroups->{$s_id}) {
      $subgroups->{$s_id}->{'exists'} = 1;
      next;
    }
    $subgroups->{$s_id}->{'new'} = 1;
    $subgroups->{$s_id}->{'name'} = $tr_subgroups->{$s_id}->{'name'};
  }

  # Deal with new subgroups.
  # XXX: not written yet.
  
  # Get testcases for each subgroup.
  foreach my $tr_g_id (keys %$subgroups) {
    $sql="SELECT test_id,summary,steps,expected_results,author_id,version,testrunner_case_id,testrunner_case_version FROM tests WHERE subgroup_id=?";
    $sth = $litmus_dbh->prepare($sql);
    $sth->execute($subgroups->{$tr_g_id}->{'s_id'});
    my @testcases;
    while (my $testcase = $sth->fetchrow_hashref) {
      push @testcases, $testcase;
    }
    $sth->finish;

    foreach my $testcase (@testcases) {
      if (!$testcase->{'testrunner_case_id'}) {
        print "# No TR info for testcase ID: " . $testcase->{'test_id'} . ", " . $testcase->{'summary'} . ", tr_g_id: $tr_g_id\n";
        next;
      }
      
      $sql="SELECT tct.case_id,tct.case_version,tct.summary,tct.action,tct.effect FROM test_cases_texts tct, test_cases t WHERE t.case_id=tct.case_id AND t.case_id=? ORDER BY tct.case_version DESC LIMIT 1";
      $sth = $tr_dbh->prepare($sql);
      $sth->execute($testcase->{'testrunner_case_id'});
      my $tr_testcase = $sth->fetchrow_hashref;
      $sth->finish;
      if (!$tr_testcase) {
        print "# No TR testcase found for case ID: " . $testcase->{'testrunner_case_id'} . ", subgroup_id: " . $subgroups->{$tr_g_id}->{'s_id'} . ", testgroup_id: $id\n";
        next;
      }

      # Compare our two testcases.
      next if ($testcase->{'testrunner_case_version'} == $tr_testcase->{'case_version'});
      
      # If the corresponding test has been update in both Litmus _and_ 
      # Testrunner, warn the user, give them enough info to easily fix the
      # problem manually, and continue to the next testcase without updating.
      if ($testcase->{'version'} > $testcase->{'testrunner_case_version'} and
          $tr_testcase->{'case_version'} > $testcase->{'testrunner_case_version'}) {
        print "# Testcase update collision detected.\n";
        print "Litmus testcase ID#: " . $testcase->{'test_id'} . "; TR case ID#: " . $tr_testcase->{'case_id'} . "\n";
        next;
      }
      
      $tr_testcase->{'summary'} =~ s/'/\\'/g;
      $tr_testcase->{'action'} =~ s/'/\\'/g;
      $tr_testcase->{'effect'} =~ s/'/\\'/g;
      $tr_testcase->{'summary'} =~ s/\&amp;/\&/g;
      $tr_testcase->{'action'} =~ s/\&amp;/\&/g;
      $tr_testcase->{'effect'} =~ s/\&amp;/\&/g;
      $tr_testcase->{'summary'} =~ s/^\s+//g;
      $tr_testcase->{'action'} =~ s/^\s+//g;
      $tr_testcase->{'effect'} =~ s/^\s+//g;
      $tr_testcase->{'summary'} =~ s/\s+$//g;
      $tr_testcase->{'action'} =~ s/\s+$//g;
      $tr_testcase->{'effect'} =~ s/\s+$//g;
      my $update_cmd = "UPDATE tests SET version=" . $tr_testcase->{'case_version'} .
        ",testrunner_case_version=" . $tr_testcase->{'case_version'} .
          ",summary='" . $tr_testcase->{'summary'} .
            "',steps='" . $tr_testcase->{'action'} .
              "',expected_results='" . $tr_testcase->{'effect'} .
                "',last_updated=NOW() WHERE test_id=" . 
                  $testcase->{'test_id'} . ";\n";
      print $update_cmd;
      $testcases_updated++;
    }
    
  }
  
}

print "# Testcases updated: $testcases_updated\n";

exit;

#########################################################################
sub usage() {
    print "Usage: ./update_litmus_from_testrunner.pl\n\n";
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







