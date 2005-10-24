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
 # Portions created by the Initial Developer are Copyright (C) 2005
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

Litmus::DB::Subgroup->columns(All => qw/subgroup_id testgroup_id name/);

Litmus::DB::Subgroup->column_alias("subgroup_id", "subgroupid");
Litmus::DB::Subgroup->column_alias("testgroup_id", "testgroup");

Litmus::DB::Subgroup->has_a(testgroup => "Litmus::DB::Testgroup");

Litmus::DB::Subgroup->has_many(tests => "Litmus::DB::Test");

#########################################################################
sub community_coverage() {
  my $self = shift;
  my $platform = shift;
  my $build_id = shift;
  my $community_only = shift;
  
  my @tests;
  if (! $community_only) {    
    @tests = Litmus::DB::Test->search(
                                      subgroup => $self,
                                      status => Litmus::DB::Status->search(name => "Enabled"),
                                     );    
  } else {
    @tests = Litmus::DB::Test->search(
                                      subgroup => $self,
                                      status => Litmus::DB::Status->search(name => "Enabled"),
                                      communityenabled => 1,
                                     );    
  }
  if (@tests == 0) { return "N/A" }
  my $num_completed = 0;
  foreach my $curtest (@tests) {
    if ($curtest->is_completed($platform,$build_id)) {
      $num_completed++;
    }
  }
  
  my $result = $num_completed/(scalar @tests) * 100;
  unless ($result) {                   
    return "0";
  }

  return sprintf("%d",$result);  
}

#########################################################################
sub personal_coverage() {
  my $self = shift;
  my $platform = shift;
  my $build_id = shift;
  my $community_only = shift;
  my $user = shift;
  
  my @tests;
  if (! $community_only) {    
    @tests = Litmus::DB::Test->search(
                                      subgroup => $self,
                                      status => Litmus::DB::Status->search(name => "Enabled"),
                                     );    
  } else {
    @tests = Litmus::DB::Test->search(
                                      subgroup => $self,
                                      status => Litmus::DB::Status->search(name => "Enabled"),
                                      communityenabled => 1,
                                     );    
  }
  if (@tests == 0) { return "N/A" }
  my $num_completed = 0;
  foreach my $curtest (@tests) {
    if ($curtest->is_completed($platform,$build_id,$user)) {
      $num_completed++;
    }
  }
  
  my $result = $num_completed/(scalar @tests) * 100;
  unless ($result) {                   
    return "0";
  }

  return sprintf("%d",$result);  
}

1;




