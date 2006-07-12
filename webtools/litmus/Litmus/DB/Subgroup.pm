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
Litmus::DB::Subgroup->columns(Essential => qw/subgroup_id name testrunner_group_id enabled product_id/);

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

#########################################################################
sub clone() {
  my $self = shift;

  my $new_subgroup = $self->copy;
  if (!$new_subgroup) { 
    return undef;
  }

  # Propagate testgroup membership;
  my $dbh = __PACKAGE__->db_Main();  
  my $sql = "INSERT INTO subgroup_testgroups (subgroup_id,testgroup_id,sort_order) SELECT ?,testgroup_id,sort_order FROM subgroup_testgroups WHERE subgroup_id=?";
  
  my $rows = $dbh->do($sql,
		      undef,
		      $new_subgroup->subgroup_id,
		      $self->subgroup_id
		     );
  if (! $rows) {
    # XXX: Do we need to throw a warning here?
    # What happens when we clone a subgroup that doesn't belong to  
    # any testgroups?
  }  

  $sql = "INSERT INTO testcase_subgroups (testcase_id,subgroup_id,sort_order) SELECT testcase_id,?,sort_order FROM testcase_subgroups WHERE subgroup_id=?";
  
  $rows = $dbh->do($sql,
                   undef,
                   $new_subgroup->subgroup_id,
                   $self->subgroup_id
                  );
  if (! $rows) {
    # XXX: Do we need to throw a warning here?
  }  

  return $new_subgroup;
}

#########################################################################
sub delete_from_testgroups() {
  my $self = shift;
  
  my $dbh = __PACKAGE__->db_Main();  
  my $sql = "DELETE from subgroup_testgroups WHERE subgroup_id=?";
  return $dbh->do($sql,
                  undef,
                  $self->subgroup_id
                 );
}

#########################################################################
sub delete_from_testgroup() {
  my $self = shift;
  my $testgroup_id = shift;
  
  my $dbh = __PACKAGE__->db_Main();  
  my $sql = "DELETE from subgroup_testgroups WHERE subgroup_id=? AND testgroup_id=?";
  return $dbh->do($sql,
                  undef,
                  $self->subgroup_id,
                  $testgroup_id
                 );
}

#########################################################################
sub delete_from_testcases() {
  my $self = shift;
  
  my $dbh = __PACKAGE__->db_Main();  
  my $sql = "DELETE from testcase_subgroups WHERE subgroup_id=?";
  return $dbh->do($sql,
                  undef,
                  $self->subgroup_id
                 );
}

#########################################################################
sub delete_with_refs() {
  my $self = shift;
  $self->delete_from_testgroups();
  $self->delete_from_testcases();
  return $self->delete;
}

#########################################################################
sub update_testgroups() {
  my $self = shift;
  my $new_testgroup_ids = shift;
  
  if (scalar @$new_testgroup_ids) {
    # Failing to delete testgroups is _not_ fatal when adding a new subgroup.
    my $rv = $self->delete_from_testgroups();
    my $dbh = __PACKAGE__->db_Main();  
    my $sql = "INSERT INTO subgroup_testgroups (subgroup_id,testgroup_id,sort_order) VALUES (?,?,1)";
    foreach my $new_testgroup_id (@$new_testgroup_ids) {
      my $rows = $dbh->do($sql, 
			  undef,
			  $self->subgroup_id,
			  $new_testgroup_id
			 );
    }
  }
}

#########################################################################
sub update_testgroup() {
  my $self = shift;
  my $testgroup_id = shift;
  my $sort_order = shift;
  
  # Sort order defaults to 1.
  if (!$sort_order) {
    $sort_order = 1;
  }

  my $rv = $self->delete_from_testgroup($testgroup_id);
  my $dbh = __PACKAGE__->db_Main();  
  my $sql = "INSERT INTO subgroup_testgroups (subgroup_id,testgroup_id,sort_order) VALUES (?,?,?)";
  return $dbh->do($sql, 
                  undef,
                  $self->subgroup_id,
                  $testgroup_id,
                  $sort_order
                 );
}

#########################################################################
sub update_testcases() {
  my $self = shift;
  my $new_testcase_ids = shift;
  
  if (scalar @$new_testcase_ids) {
    # Failing to delete testcases is _not_ fatal when adding a new subgroup.
    my $rv = $self->delete_from_testcases();
    my $dbh = __PACKAGE__->db_Main();  
    my $sql = "INSERT INTO testcase_subgroups (testcase_id,subgroup_id,sort_order) VALUES (?,?,?)";
    my $sort_order = 1;
    foreach my $new_testcase_id (@$new_testcase_ids) {
      next if (!$new_testcase_id);
      # Log any failures/duplicate keys to STDERR.
      eval {
        my $rows = $dbh->do($sql, 
                            undef,
                            $new_testcase_id,
                            $self->subgroup_id,
                            $sort_order
                           );
      };
      if ($@) {
        print STDERR $@;
      }
      $sort_order++;
    }
  }
}

1;
