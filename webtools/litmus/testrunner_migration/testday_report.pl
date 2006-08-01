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
	    );

END {
  if ($dbh) { $dbh->disconnect; }
}

my $start_ts;
my $finish_ts;
my $testgroup_id;
my $build_id;
my $branch_id;
my $locale;
GetOptions('start_ts=i' => \$start_ts,
           'finish_ts=i' => \$finish_ts,
           'testgroup_id=s' => \$testgroup_id,
           'build_id=i' => \$build_id,
           'branch_id=i' => \$branch_id,
           'locale=s' => \$locale,
          );

if (
    !$start_ts or $start_ts eq '' or   
    !$finish_ts or $finish_ts eq ''
   ) {
    &usage;
    die;
}

$dbh = Litmus::DBI->db_Main;
if (!$dbh) {
  die "Unable to connect to database!";
}

my $locale_sql_select = "SELECT tr.locale_abbrev,count(tr.testresult_id)";
my $locale_sql_from = "FROM test_results tr";
my $locale_sql_where ="WHERE tr.submission_time>=$start_ts and tr.submission_time<$finish_ts";
my $locale_sql_group_by = "GROUP BY tr.locale_abbrev";

my $platform_sql_select = "SELECT pl.name,count(tr.testresult_id)";
my $platform_sql_from = "FROM test_results tr, testcases t, platforms pl, opsyses o";
my $platform_sql_where = "WHERE tr.testcase_id=t.testcase_id AND tr.submission_time>=$start_ts and tr.submission_time<$finish_ts AND tr.opsys_id=o.opsys_id AND o.platform_id=pl.platform_id";
my $platform_sql_group_by = "GROUP BY o.platform_id";

my $status_sql_select = "SELECT rs.name,count(tr.testresult_id)";
my $status_sql_from = "FROM test_results tr, test_result_status_lookup rs";
my $status_sql_where = "WHERE tr.submission_time>=$start_ts and tr.submission_time<$finish_ts AND rs.result_status_id=tr.result_status_id";
my $status_sql_group_by = "GROUP BY tr.result_status_id";

my $subgroup_sql_select = "SELECT CONCAT(p.name,':',tg.name,':',s.name) as name,count(tr.testresult_id),sgtg.subgroup_id";
my $subgroup_sql_from = "FROM test_results tr, testcases t, testcase_subgroups tsg, subgroups s, subgroup_testgroups sgtg, testgroups tg, products p";
my $subgroup_sql_where = "WHERE tr.submission_time>=$start_ts and tr.submission_time<$finish_ts AND tg.product_id=p.product_id AND tg.testgroup_id=sgtg.testgroup_id AND sgtg.subgroup_id=s.subgroup_id AND tsg.subgroup_id=s.subgroup_id AND tsg.testcase_id=t.testcase_id AND tr.testcase_id=t.testcase_id";
my $subgroup_sql_group_by = "GROUP BY tg.product_id,tg.testgroup_id,s.subgroup_id";
my $subgroup_sql_order_by = "ORDER BY p.name ASC, tg.name ASC, sgtg.sort_order ASC";

my $user_sql_select = "SELECT u.email,count(tr.testresult_id) AS num_results,u.irc_nickname";
my $user_sql_from = "FROM test_results tr, testcases t, users u";
my $user_sql_where = "WHERE tr.testcase_id=t.testcase_id AND tr.submission_time>=$start_ts and tr.submission_time<$finish_ts AND tr.user_id=u.user_id";
my $user_sql_group_by = "GROUP BY tr.user_id";
my $user_sql_order_by = "ORDER BY num_results DESC";

my $tester_sql_select = "SELECT u.email,rs.name AS result_status,count(rs.name) as num_results,u.irc_nickname";
my $tester_sql_from = "FROM test_results tr, users u, test_result_status_lookup rs";
my $tester_sql_where = "WHERE tr.submission_time>=$start_ts and tr.submission_time<$finish_ts AND tr.user_id=u.user_id AND rs.result_status_id=tr.result_status_id";
my $tester_sql_group_by = "GROUP BY tr.user_id,rs.name";
my $tester_sql_order_by = "ORDER BY u.email";

if ($testgroup_id) {
  $locale_sql_from .= ", testcase_subgroups tsg, subgroup_testgroups sgtg";
  $locale_sql_where .= " AND tr.testcase_id=tsg.testcase_id AND tsg.subgroup_id=sgtg.subgroup_id AND sgtg.testgroup_id=$testgroup_id";

  $platform_sql_from .= ",  testcase_subgroups tsg, subgroup_testgroups sgtg";
  $platform_sql_where .= " AND tr.testcase_id=tsg.testcase_id AND tsg.subgroup_id=sgtg.subgroup_id AND sgtg.testgroup_id=$testgroup_id";

  $status_sql_from .= ",  testcase_subgroups tsg, subgroup_testgroups sgtg";
  $status_sql_where .= " AND tr.testcase_id=tsg.testcase_id AND tsg.subgroup_id=sgtg.subgroup_id AND sgtg.testgroup_id=$testgroup_id";

  $subgroup_sql_where .= " AND tg.testgroup_id=$testgroup_id";

  $user_sql_from .= ",  testcase_subgroups tsg, subgroup_testgroups sgtg";
  $user_sql_where .= " AND tr.testcase_id=tsg.testcase_id AND tsg.subgroup_id=sgtg.subgroup_id AND sgtg.testgroup_id=$testgroup_id";
 
  $tester_sql_from .= ",  testcase_subgroups tsg, subgroup_testgroups sgtg";
  $tester_sql_where .= " AND tr.testcase_id=tsg.testcase_id AND tsg.subgroup_id=sgtg.subgroup_id AND sgtg.testgroup_id=$testgroup_id";
}

if ($build_id) {
  $locale_sql_where .= " AND tr.build_id LIKE '$build_id%'";
  $platform_sql_where .= " AND tr.build_id LIKE '$build_id%'";
  $status_sql_where .= " AND tr.build_id LIKE '$build_id%'";
  $subgroup_sql_where .= " AND tr.build_id LIKE '$build_id%'";
  $user_sql_where .= " AND tr.build_id LIKE '$build_id%'";
  $tester_sql_where .= " AND tr.build_id LIKE '$build_id%'";
}

if ($branch_id) {
  $locale_sql_where .= " AND tr.branch_id=$branch_id";
  $platform_sql_where .= " AND tr.branch_id=$branch_id";
  $status_sql_where .= " AND tr.branch_id=$branch_id";
  $subgroup_sql_where .= " AND tr.branch_id=$branch_id";
  $user_sql_where .= " AND tr.branch_id=$branch_id";
  $tester_sql_where .= " AND tr.branch_id=$branch_id";
}

if ($locale) {
  $locale_sql_where .= " AND tr.locale_abbrev=$locale";
  $platform_sql_where .= " AND tr.locale_abbrev=$locale";
  $status_sql_where .= " AND tr.locale_abbrev=$locale";
  $subgroup_sql_where .= " AND tr.locale_abbrev=$locale";
  $user_sql_where .= " AND tr.locale_abbrev=$locale";
  $tester_sql_where .= " AND tr.locale_abbrev=$locale";
}

my $locale_sql = "$locale_sql_select $locale_sql_from $locale_sql_where $locale_sql_group_by";
my $platform_sql = "$platform_sql_select $platform_sql_from $platform_sql_where $platform_sql_group_by";
my $status_sql = "$status_sql_select $status_sql_from $status_sql_where $status_sql_group_by";
my $subgroup_sql = "$subgroup_sql_select $subgroup_sql_from $subgroup_sql_where $subgroup_sql_group_by $subgroup_sql_order_by";
my $user_sql = "$user_sql_select $user_sql_from $user_sql_where $user_sql_group_by $user_sql_order_by";
my $tester_sql = "$tester_sql_select $tester_sql_from $tester_sql_where $tester_sql_group_by $tester_sql_order_by";

print "=== Total Test Results By Locale ===\n";
print '{| border="1" cellpadding="2"',"\n";
print '! style="background:#efefef" | \'\'\'Locale\'\'\'',"\n";
print '! style="background:#efefef" | \'\'\'# Results\'\'\'',"\n";

my $sth = $dbh->prepare($locale_sql);
$sth->execute();

while (my @result = $sth->fetchrow_array) {
  print "|-\n";
  print "| " . $result[0] . "\n";
  print "| " . $result[1] . "\n";
}
$sth->finish;
print "|}\n\n";

print "=== Total Test Results By Platform ===\n\n";
print '{| border="1" cellpadding="2"',"\n";
print '! style="background:#efefef" | \'\'\'Platform\'\'\'',"\n";
print '! style="background:#efefef" | \'\'\'# Results\'\'\'',"\n";

$sth = $dbh->prepare($platform_sql);
$sth->execute();

while (my @result = $sth->fetchrow_array) {
  print "|-\n";
  print "| " . $result[0] . "\n";
  print "| " . $result[1] . "\n";
}
$sth->finish;
print "|}\n\n";

print "=== Total Test Results By Result Status ===\n\n";
print '{| border="1" cellpadding="2"',"\n";
print '! style="background:#efefef" | \'\'\'Result Status\'\'\'',"\n";
print '! style="background:#efefef" | \'\'\'# Results\'\'\'',"\n";

$sth = $dbh->prepare($status_sql);
$sth->execute();

while (my @result = $sth->fetchrow_array) {
  print "|-\n";
  print "| " . $result[0] . "\n";
  print "| " . $result[1] . "\n";
}
$sth->finish;
print "|}\n\n";

print "=== Total Test Results By Subgroup ===\n";
print '{| border="1" cellpadding="2"',"\n";
print '! style="background:#efefef" | \'\'\'Subgroup\'\'\'',"\n";
print '! style="background:#efefef" | \'\'\'# Results\'\'\'',"\n";
print '! style="background:#efefef" | \'\'\'# Testcases in Subgroup\'\'\'',"\n";

$sth = $dbh->prepare($subgroup_sql);
$sth->execute();

my @results;
while (my @result = $sth->fetchrow_array) {
  push @results, \@result;
}
$sth->finish;

my $subgroup_count_sql = "SELECT COUNT(t.testcase_id) FROM testcases t, testcase_subgroups tsg WHERE tsg.subgroup_id=? AND t.testcase_id=tsg.testcase_id AND t.enabled=1 AND t.community_enabled=1";
$sth = $dbh->prepare($subgroup_count_sql);
foreach my $result (@results) {
  $sth->execute($result->[2]);
  my ($subgroup_count) = $sth->fetchrow_array;
  print "|-\n";
  print "| " . $result->[0] . "\n";
  print "| " . $result->[1] . "\n";
  print "| $subgroup_count\n";
  next;
}
$sth->finish;
print "|}\n\n";

print "==== Top Testers ====\n\n";
print '{| border="1" cellpadding="2"',"\n";
print '! style="background:#efefef" | \'\'\'Tester\'\'\'',"\n";
print '! style="background:#efefef" | \'\'\'# Results\'\'\'',"\n";

$sth = $dbh->prepare($user_sql);
$sth->execute();

my $users;
while (my @result = $sth->fetchrow_array) {
  my $email = $result[2];
  $email =~ s/\@/\&\#64;/;
  
  print "|-\n";
  print "| " . $email . "\n";
  print "| " . $result[1] . "\n";
}
$sth->finish;
print "|}\n\n";

$sth = $dbh->prepare($tester_sql);
$sth->execute();

my $testers;
while (my @result = $sth->fetchrow_array) {
  $testers->{$result[3]}->{$result[1]} = $result[2];
}

print "==== Result Status Breakdown By Tester ====\n\n";
print '{| border="1" cellpadding="2"',"\n";
print '! style="background:#efefef" | \'\'\'Tester\'\'\'',"\n";
print '! style="background:#efefef" | \'\'\'Pass\'\'\'',"\n";
print '! style="background:#efefef" | \'\'\'Fail\'\'\'',"\n";
print '! style="background:#efefef" | \'\'\'Unclear\'\'\'',"\n";

foreach my $key (sort keys %$testers) {
  my $email = $key;
  $email =~ s/\@/\&\#64;/;
  
  print "|-\n";
  print "| " . $email . "\n";
  print "| ";
  if ($testers->{$key}->{'Pass'}) {
    print $testers->{$key}->{'Pass'};
  } else {
    print "0";
  }
  print "\n";
  print "| ";
  if ($testers->{$key}->{'Fail'}) {
    print $testers->{$key}->{'Fail'};
  } else {
    print "0";
  }
  print "\n";
  print "| ";
  if ($testers->{$key}->{'Test unclear/broken'}) {
    print $testers->{$key}->{'Test unclear/broken'};
  } else {
    print "0";
  }
  print "\n";
}
$sth->finish;
print "|}\n\n";

exit;

#########################################################################
sub usage() {
    print<<EOS
Usage: ./testday_report.pl --start_ts=YYYYMMDDhhmmss
                           --finish_ts=YYYYMMDDhhmmss
                           [--testgroup_id=#]
                           [--build_id=#]
                           [--branch_id=#]
                           [--locale=locale_abbrev]
EOS

}


