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

package Litmus::DB::Testgroup;

use strict;
use base 'Litmus::DBI';

Litmus::DB::Testgroup->table('testgroups');

Litmus::DB::Testgroup->columns(All => qw/testgroup_id product_id name enabled testrunner_plan_id/);
Litmus::DB::Testgroup->columns(Essential => qw/testgroup_id product_id name enabled testrunner_plan_id/);

Litmus::DB::Testgroup->column_alias("product_id", "product");

Litmus::DB::Testgroup->has_a(product => "Litmus::DB::Product");

__PACKAGE__->set_sql(EnabledByBranch => qq{
                                           SELECT tg.* 
                                           FROM testgroups tg, testgroup_branches tgb
                                           WHERE tgb.branch_id=? AND tgb.testgroup_id=tg.testgroup_id AND tg.enabled=1 ORDER by tg.name ASC
});

__PACKAGE__->set_sql(ByBranch => qq{
                                    SELECT tg.* 
                                    FROM testgroups tg, testgroup_branches tgb
                                    WHERE tgb.branch_id=? AND tgb.testgroup_id=tg.testgroup_id ORDER by tg.name ASC
});

__PACKAGE__->set_sql(EnabledBySubgroup => qq{
                                             SELECT tg.* 
                                             FROM testgroups tg, subgroup_testgroups sgtg
                                             WHERE sgtg.subgroup_id=? AND sgtg.testgroup_id=tg.testgroup_id AND tg.enabled=1 ORDER by tg.name ASC
});

__PACKAGE__->set_sql(BySubgroup => qq{
                                      SELECT tg.* 
                                      FROM testgroups tg, subgroup_testgroups sgtg
                                      WHERE sgtg.subgroup_id=? AND sgtg.testgroup_id=tg.testgroup_id ORDER by tg.name ASC
});

__PACKAGE__->set_sql(EnabledByTestcase => qq{
                                             SELECT tg.* 
                                             FROM testgroups tg, subgroup_testgroups sgtg, testcase_subgroups tcsg
                                             WHERE tcsg.testcase_id=? AND tcsg.subgroup_id=sgtg.subgroup_id AND sgtg.testgroup_id=tg.testgroup_id AND tg.enabled=1 ORDER by tg.name ASC
});

__PACKAGE__->set_sql(ByTestcase => qq{
                                      SELECT tg.* 
                                      FROM testgroups tg, subgroup_testgroups sgtg, testcase_subgroups tcsg
                                      WHERE tcsg.testcase_id=? AND tcsg.subgroup_id=sgtg.subgroup_id AND sgtg.testgroup_id=tg.testgroup_id ORDER by tg.name ASC
});

#########################################################################
sub coverage {
  my $self = shift;
  my $platform = shift;
  my $build_id = shift;
  my $locale = shift;
  my $community_only = shift;
  my $user = shift;

  my $percent_completed = 0;

  my @subgroups = Litmus::DB::Subgroup->search_EnabledByTestgroup($self->testgroup_id);
  my $num_empty_subgroups = 0;
  foreach my $subgroup (@subgroups) {
    my $subgroup_percent = $subgroup->coverage(
                                               $platform, 
                                               $build_id, 
                                               $locale,
                                               $community_only,
                                               $user,
                                              );
    if ($subgroup_percent eq "N/A") {
      $num_empty_subgroups++;
    } else {
      $percent_completed += $subgroup_percent;
    }
  }
  
  if (scalar(@subgroups) - $num_empty_subgroups == 0) { 
    return "N/A"
  }
  my $total_percentage = $percent_completed / 
    (scalar @subgroups - $num_empty_subgroups);
  
  return sprintf("%d",$total_percentage);
}

#########################################################################
sub clone() {
  my $self = shift;

  my $new_testgroup = $self->copy;
  if (!$new_testgroup) {
    return undef;
  }

  # Propagate testgroup membership;
  my $dbh = __PACKAGE__->db_Main();
  my $sql = "INSERT INTO subgroup_testgroups (subgroup_id,testgroup_id,sort_order) SELECT subgroup_id,?,sort_order FROM subgroup_testgroups WHERE testgroup_id=?";

  my $rows = $dbh->do($sql,
		      undef,
		      $new_testgroup->testgroup_id,
		      $self->testgroup_id
		     );
  if (! $rows) {
    # XXX: Do we need to throw a warning here?
  }  

  return $new_testgroup;
}

#########################################################################
sub delete_from_subgroups() {
  my $self = shift;

  my $dbh = __PACKAGE__->db_Main();  
  my $sql = "DELETE from subgroup_testgroups WHERE testgroup_id=?";
  my $rows = $dbh->do($sql,
                      undef,
                      $self->testgroup_id
                     );
}

#########################################################################
sub delete_from_test_runs() {
  my $self = shift;

  # XXX: Placeholder for test runs.
  return;
}

#########################################################################
sub delete_with_refs() {
  my $self = shift;
  $self->delete_from_subgroups();
  $self->delete_from_test_runs();
  return $self->delete;
}

#########################################################################
sub update_subgroups() {
  my $self = shift;
  my $new_subgroup_ids = shift;
  
  if (scalar @$new_subgroup_ids) {
    # Failing to delete subgroups is _not_ fatal when adding a new testgroup.
    my $rv = $self->delete_from_subgroups();
    my $dbh = __PACKAGE__->db_Main();  
    my $sql = "INSERT INTO subgroup_testgroups (subgroup_id,testgroup_id,sort_order) VALUES (?,?,?)";
    my $sort_order = 1;
    foreach my $new_subgroup_id (@$new_subgroup_ids) {
      next if (!$new_subgroup_id);
      # Log any failures/duplicate keys to STDERR.
      eval {
        my $rows = $dbh->do($sql, 
                            undef,
                            $new_subgroup_id,
                            $self->testgroup_id,
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


1;








