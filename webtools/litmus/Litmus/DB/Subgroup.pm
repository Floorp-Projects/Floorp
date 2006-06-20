# -*- mode: cperl; c-basic-offset: 8; indent-tabs-mode: nil; -*-

=head1 COPYRIGHT

 # ***** BEGIN LICENSE BLOCK *****
 # Version: MPL 1.1
 #
 # The contents of this file are subject to the Mozilla Public License
 # Version 1.1 (the "License"); you may not use this file except in
 # compliance with the License. You may obtain a copy of the License
 # at http://www.mozilla.org/MPL/
 #
 # Software distributed under the License is distributed on an "AS IS"
 # basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 # the License for the specific language governing rights and
 # limitations under the License.
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

=cut

package Litmus::DB::Subgroup;

use strict;
use base 'Litmus::DBI';

use Time::Piece::MySQL;
use Litmus::DB::Testresult;

Litmus::DB::Subgroup->table('subgroups');

Litmus::DB::Subgroup->columns(All => qw/subgroup_id name testrunner_group_id enabled product_id/);

Litmus::DB::Subgroup->column_alias("subgroup_id", "subgroupid");

Litmus::DB::Subgroup->has_a(product => "Litmus::DB::Product");

__PACKAGE__->set_sql(EnabledByTestgroup => qq{
                                              SELECT sg.* 
                                              FROM subgroups sg, subgroup_testgroups sgtg 
                                              WHERE sgtg.testgroup_id=? AND sgtg.subgroup_id=sg.subgroup_id AND sg.enabled=1 
                                              ORDER BY sgtg.sort_order ASC
});

__PACKAGE__->set_sql(ByTestgroup => qq{
                                       SELECT sg.* 
                                       FROM subgroups sg, subgroup_testgroups sgtg 
                                       WHERE sgtg.testgroup_id=? AND sgtg.subgroup_id=sg.subgroup_id
                                       ORDER BY sgtg.sort_order ASC
});

__PACKAGE__->set_sql(NumEnabledTestcases => qq{
                                               SELECT count(tc.testcase_id) as num_testcases
                                               FROM testcases tc, testcase_subgroups tcsg
                                               WHERE tcsg.subgroup_id=? AND tcsg.testcase_id=tc.testcase_id AND tc.enabled=1 AND tc.community_enabled=1 
});

__PACKAGE__->set_sql(EnabledByTestcase => qq{
                                             SELECT sg.* 
                                             FROM subgroups sg, testcase_subgroups tcsg
                                             WHERE tcsg.testcase_id=? AND tcsg.subgroup_id=sg.subgroup_id AND sg.enabled=1 
                                             ORDER by sg.name ASC
});

__PACKAGE__->set_sql(ByTestcase => qq{
                                      SELECT sg.* 
                                      FROM subgroups sg, testcase_subgroups tcsg
                                      WHERE tcsg.testcase_id=? AND tcsg.subgroup_id=sg.subgroup_id 
                                      ORDER by sg.name ASC
});

#########################################################################
sub coverage() {
  my $self = shift;
  my $platform = shift;
  my $build_id = shift;
  my $locale = shift;
  my $community_only = shift;
  my $user = shift;

  my $sql = "SELECT COUNT(t.testcase_id) FROM testcase_subgroups tsg, testcases t WHERE tsg.subgroup_id=? AND tsg.testcase_id=t.testcase_id AND t.enabled=1";
  if ($community_only) {
    $sql .= " AND t.community_enabled=1";
  }
  my $dbh = $self->db_Main();
  my $sth = $dbh->prepare_cached($sql);
  $sth->execute(
                $self->{'subgroup_id'},
               );
  my ($num_testcases) = $sth->fetchrow_array;

  $sth->finish;

  if (!$num_testcases or 
      $num_testcases == 0) { return "N/A" }


  $sql = "SELECT t.testcase_id, count(tr.testresult_id) AS num_results
             FROM testcase_subgroups tsg JOIN testcases t ON (tsg.testcase_id=t.testcase_id) LEFT JOIN test_results tr ON (tr.testcase_id=t.testcase_id) JOIN opsyses o ON (tr.opsys_id=o.opsys_id)
             WHERE tsg.subgroup_id=? AND tr.build_id=? AND tr.locale_abbrev=? AND o.platform_id=?";
  if ($community_only) {
    $sql .= " AND t.community_enabled=1";
  }
  if ($user) {
    $sql .= " AND tr.user_id=" . $user->{'user_id'};
  }
  
  $sql .= " GROUP BY tr.testcase_id";

  $sth = $dbh->prepare_cached($sql);
  $sth->execute(
                $self->{'subgroup_id'},
                $build_id,
                $locale,
                $platform->{'platform_id'}
               );
  my @test_results = $self->sth_to_objects($sth);

  $sth->finish;

  if (@test_results == 0) { return "0" }

  my $num_completed = 0;
  foreach my $curtest (@test_results) {
    if ($curtest->{'num_results'} > 0) {
      $num_completed++;
    }
  }
  
  my $result = $num_completed/($num_testcases) * 100;
  unless ($result) {                   
    return "0";
  }

  return sprintf("%d",$result);  
}

1;
