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

Litmus::DB::Testgroup->column_alias("product_id", "product");

Litmus::DB::Testgroup->has_a(product => "Litmus::DB::Product");

__PACKAGE__->set_sql(EnabledByBranch => qq{
                                           SELECT tg.* 
                                           FROM testgroups tg, testgroup_branches tgb
                                           WHERE tgb.branch_id=? AND tgb.testgroup_id=tg.testgroup_id AND tg.enabled=1 ORDER by tg.name ASC
});

__PACKAGE__->set_sql(EnabledByTestcase => qq{
                                             SELECT tg.* 
                                             FROM testgroups tg, subgroup_testgroups sgtg, testcase_subgroups tcsg
                                             WHERE tcsg.testcase_id=? AND tcsg.subgroup_id=sgtg.subgroup_id AND sgtg.testgroup_id=tg.testgroup_id AND tg.enabled=1 ORDER by tg.name ASC
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

1;








