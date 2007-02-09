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
 #   Zach Lipton <zach@zachlipton.com>
 #
 # ***** END LICENSE BLOCK *****

=cut

package Litmus::TestEvent; 
$VERSION = 1.00;
use strict;

use Litmus::DBI;
use Litmus::DB::TestDay;

use Date::Manip;

sub new {
  my ($class, %args) = @_;
  my $self = bless {}, ref($class)||$class;
  $self->_init(%args);
  return $self;
}

sub _init {
  my ($self, %args) = @_;
  
  if (!$args{testday_id} and
      !$args{testdate} and 
      !($args{start_timestamp} and $args{finish_timestamp})) {
    warn "Error initializing TestEvent - please supply either a testday_id, testdate or a start_timestamp/finish_timestamp pair.";
    return 1;
  }

  $self->{_dbh} = Litmus::DBI->db_Main;

  if ($args{testday_id}) {
    my $testday = Litmus::DB::TestDay->retrieve($args{testday_id});

    if ($testday) {
      $self->{_planned_testday} = 1;
      
      $self->{_start_timestamp} = &UnixDate($testday->start_timestamp,"%q");
      $self->{_finish_timestamp} = &UnixDate($testday->finish_timestamp,"%q");
      $self->{_description} = $testday->description;
      $self->{_product_id} = $testday->product_id;
      $self->{_testgroup_id} = $testday->testgroup_id;
      $self->{_build_id} = $testday->build_id;
      $self->{_branch_id} = $testday->branch_id;
      $self->{_locale} = $testday->locale_abbrev;
    } else {
      warn "Unable to lookup testday for testday_id: " . $args{testday_id};
      return 1;      
    }
    return;
  }

  if ($args{testdate}) {
    $self->{_start_timestamp} = &UnixDate($args{testdate} . " 07:00:00", "%q");
    $self->{_finish_timestamp} = &UnixDate($args{testdate} . " 17:00:00", "%q");
  } elsif ($args{start_timestamp} and $args{finish_timestamp}) {
    $self->{_start_timestamp} = &UnixDate($args{start_timestamp}, "%q");
    $self->{_finish_timestamp} = &UnixDate($args{finish_timestamp}, "%q");
  }

  $self->{_description} = "User-defined";  
  $self->{_product_id} = $args{product_id};
  $self->{_testgroup_id} = $args{testgroup_id};
  $self->{_build_id} = $args{build_id};
  $self->{_branch_id} = $args{branch_id};
  $self->{_locale} = $args{locale};
}

#########################################################################
sub getBreakdownByLocale {
  my ($self) = @_;
  my $locale_sql_select = "SELECT tr.locale_abbrev,count(tr.testresult_id) as num_results";
  my $locale_sql_from = "FROM test_results tr";
  my $locale_sql_where ="WHERE tr.submission_time>=" . $self->{_start_timestamp} . " and tr.submission_time<" . $self->{_finish_timestamp};
  my $locale_sql_group_by = "GROUP BY tr.locale_abbrev";
  my $locale_sql_order_by = "ORDER BY num_results DESC";

  if ($self->{_product_id}) {
    $locale_sql_from .= ", testcases t";
    $locale_sql_where .= " AND tr.testcase_id=t.testcase_id AND t.product_id=" . $self->{_product_id};
  }

  if ($self->{_testgroup_id}) {
    $locale_sql_from .= ", testcase_subgroups tsg, subgroup_testgroups sgtg";
    $locale_sql_where .= " AND tr.testcase_id=tsg.testcase_id AND tsg.subgroup_id=sgtg.subgroup_id AND sgtg.testgroup_id=".$self->{_testgroup_id};
  }

  if ($self->{_build_id}) {
    $locale_sql_where .= " AND tr.build_id LIKE '" . $self->{_build_id} . "%'";
  }

  if ($self->{_branch_id}) {
    $locale_sql_where .= " AND tr.branch_id=" . $self->{_branch_id};
  }

  if ($self->{_locale}) {
    $locale_sql_where .= " AND tr.locale_abbrev='" . $self->{_locale} . "'";
  }

  my $locale_sql = "$locale_sql_select $locale_sql_from $locale_sql_where $locale_sql_group_by $locale_sql_order_by";
  
  my $sth = $self->{_dbh}->prepare($locale_sql);
  $sth->execute();  

  my @locales;
  while (my $result = $sth->fetchrow_hashref) {
    push @locales, $result;
  }
  $sth->finish;

  return \@locales;
}

#########################################################################
sub getBreakdownByPlatform {
  my ($self) = @_;

  my $platform_sql_select = "SELECT pl.name,count(tr.testresult_id) AS num_results";
  my $platform_sql_from = "FROM test_results tr, testcases t, platforms pl, opsyses o";
  my $platform_sql_where = "WHERE tr.testcase_id=t.testcase_id AND tr.submission_time>=$self->{_start_timestamp} and tr.submission_time<$self->{_finish_timestamp} AND tr.opsys_id=o.opsys_id AND o.platform_id=pl.platform_id";
  my $platform_sql_group_by = "GROUP BY o.platform_id";
  my $platform_sql_order_by = "ORDER BY num_results DESC";

  if ($self->{_product_id}) {
    $platform_sql_where .= " AND t.product_id=" . $self->{_product_id};
  }

  if ($self->{_testgroup_id}) {
    $platform_sql_from .= ",  testcase_subgroups tsg, subgroup_testgroups sgtg";
    $platform_sql_where .= " AND tr.testcase_id=tsg.testcase_id AND tsg.subgroup_id=sgtg.subgroup_id AND sgtg.testgroup_id=$self->{_testgroup_id}";
  }

  if ($self->{_build_id}) {
    $platform_sql_where .= " AND tr.build_id LIKE '" . $self->{_build_id} . "%'";
  }

  if ($self->{_branch_id}) {
    $platform_sql_where .= " AND tr.branch_id=" . $self->{_branch_id};
  }

  if ($self->{_locale}) {
    $platform_sql_where .= " AND tr.locale_abbrev='" . $self->{_locale} . "'";
  }

  my $platform_sql = "$platform_sql_select $platform_sql_from $platform_sql_where $platform_sql_group_by $platform_sql_order_by";

  my $sth = $self->{_dbh}->prepare($platform_sql);
  $sth->execute();

  my @platforms;
  while (my $result = $sth->fetchrow_hashref) {
    push @platforms, $result;
  }
  $sth->finish;

  return \@platforms;
}

#########################################################################
sub getBreakdownByResultStatus {
  my ($self) = @_;

  my $status_sql_select = "SELECT rs.name,count(tr.testresult_id) AS num_results,rs.class_name";
  my $status_sql_from = "FROM test_results tr, test_result_status_lookup rs";
  my $status_sql_where = "WHERE tr.submission_time>=$self->{_start_timestamp} and tr.submission_time<$self->{_finish_timestamp} AND rs.result_status_id=tr.result_status_id";
  my $status_sql_group_by = "GROUP BY tr.result_status_id";
  my $status_sql_order_by = "ORDER BY num_results DESC";
 
  if ($self->{_product_id}) {
    $status_sql_from .= ", testcases t";
    $status_sql_where .= " AND tr.testcase_id=t.testcase_id AND t.product_id=" . $self->{_product_id};
  }

  if ($self->{_testgroup_id}) {
    $status_sql_from .= ",  testcase_subgroups tsg, subgroup_testgroups sgtg";
    $status_sql_where .= " AND tr.testcase_id=tsg.testcase_id AND tsg.subgroup_id=sgtg.subgroup_id AND sgtg.testgroup_id=" . $self->{_testgroup_id};
  }

  if ($self->{_build_id}) {
    $status_sql_where .= " AND tr.build_id LIKE '" . $self->{_build_id} . "%'";
  } 

  if ($self->{_branch_id}) {
    $status_sql_where .= " AND tr.branch_id=" . $self->{_branch_id};
  }

  if ($self->{_locale}) {
    $status_sql_where .= " AND tr.locale_abbrev='" . $self->{_locale} . "'";
  }
  
  my $status_sql = "$status_sql_select $status_sql_from $status_sql_where $status_sql_group_by $status_sql_order_by";

  my $sth = $self->{_dbh}->prepare($status_sql);
  $sth->execute();

  my @statuses;
  while (my $result = $sth->fetchrow_hashref) {
    push @statuses, $result;
  }
  $sth->finish;

  return \@statuses;
}

#########################################################################
sub getBreakdownBySubgroup {
  my ($self) = @_;

  my $subgroup_sql_select = "SELECT CONCAT(p.name,':',tg.name,':',s.name) as name,count(tr.testresult_id) as num_results,sgtg.subgroup_id";
  my $subgroup_sql_from = "FROM test_results tr, testcases t, testcase_subgroups tsg, subgroups s, subgroup_testgroups sgtg, testgroups tg, products p";
  my $subgroup_sql_where = "WHERE tr.submission_time>=$self->{_start_timestamp} and tr.submission_time<$self->{_finish_timestamp} AND tg.product_id=p.product_id AND tg.testgroup_id=sgtg.testgroup_id AND sgtg.subgroup_id=s.subgroup_id AND tsg.subgroup_id=s.subgroup_id AND tsg.testcase_id=t.testcase_id AND tr.testcase_id=t.testcase_id";
  my $subgroup_sql_group_by = "GROUP BY tg.product_id,tg.testgroup_id,s.subgroup_id";
  my $subgroup_sql_order_by = "ORDER BY num_results DESC, p.name ASC, tg.name ASC, sgtg.sort_order ASC";

  if ($self->{_product_id}) {
    $subgroup_sql_where .= " AND t.product_id=" . $self->{_product_id};
  }

  if ($self->{_testgroup_id}) {
    $subgroup_sql_where .= " AND tg.testgroup_id=" . $self->{_testgroup_id};
  }

  if ($self->{_build_id}) {
    $subgroup_sql_where .= " AND tr.build_id LIKE '" . $self->{_build_id} . "%'";
  }
  
  if ($self->{_branch_id}) {
    $subgroup_sql_where .= " AND tr.branch_id=$self->{_branch_id}";
  }
  if ($self->{_locale}) {
    $subgroup_sql_where .= " AND tr.locale_abbrev='" . $self->{_locale} . "'";
  }

  my $subgroup_sql = "$subgroup_sql_select $subgroup_sql_from $subgroup_sql_where $subgroup_sql_group_by $subgroup_sql_order_by";

  my @subgroups;
  my $sth = $self->{_dbh}->prepare($subgroup_sql);
  $sth->execute();

  while (my $result = $sth->fetchrow_hashref) {
    push @subgroups, $result;
  }
  $sth->finish;

  my $subgroup_count_sql = "SELECT COUNT(t.testcase_id) FROM testcases t, testcase_subgroups tsg WHERE tsg.subgroup_id=? AND t.testcase_id=tsg.testcase_id AND t.enabled=1 AND t.community_enabled=1";
  $sth = $self->{_dbh}->prepare($subgroup_count_sql);
  $sth->execute();

  foreach my $result (@subgroups) {
    $sth->execute($result->{'subgroup_id'});
    my ($testcase_count) = $sth->fetchrow_array;
    $result->{'testcase_count'} = $testcase_count;
  }
  $sth->finish;

  return \@subgroups;
}

#########################################################################
sub getBreakdownByUser {
  my ($self) = @_;

  my $user_sql_select = "SELECT u.user_id,u.email,count(tr.testresult_id) AS num_results,u.irc_nickname";
  my $user_sql_from = "FROM test_results tr, testcases t, users u";
  my $user_sql_where = "WHERE tr.testcase_id=t.testcase_id AND tr.submission_time>=$self->{_start_timestamp} and tr.submission_time<$self->{_finish_timestamp} AND tr.user_id=u.user_id";
  my $user_sql_group_by = "GROUP BY tr.user_id";
  my $user_sql_order_by = "ORDER BY num_results DESC";

  if ($self->{_product_id}) {
    $user_sql_where .= " AND t.product_id=" . $self->{_product_id};
  }

  if ($self->{_testgroup_id}) {
    $user_sql_from .= ",  testcase_subgroups tsg, subgroup_testgroups sgtg";
    $user_sql_where .= " AND tr.testcase_id=tsg.testcase_id AND tsg.subgroup_id=sgtg.subgroup_id AND sgtg.testgroup_id=" . $self->{_testgroup_id};
  }

  if ($self->{_build_id}) {
    $user_sql_where .= " AND tr.build_id LIKE '" . $self->{_build_id} . "%'";
  }

  if ($self->{_branch_id}) {
    $user_sql_where .= " AND tr.branch_id=" . $self->{_branch_id};
  }
  if ($self->{_locale}) {
    $user_sql_where .= " AND tr.locale_abbrev='" . $self->{_locale} . "'";
  }

  my $user_sql = "$user_sql_select $user_sql_from $user_sql_where $user_sql_group_by $user_sql_order_by";

  my $sth = $self->{_dbh}->prepare($user_sql);
  $sth->execute();

  my @users;
  while (my $result = $sth->fetchrow_hashref) {
    push @users, $result;
  }
  $sth->finish;

  return \@users;
}

#########################################################################
sub getBreakdownByUserAndResultStatus {
  my ($self) = @_;
  
  my $tester_sql_select = "SELECT u.email,rs.class_name AS result_status,count(rs.name) as num_results,u.irc_nickname";
  my $tester_sql_from = "FROM test_results tr, users u, test_result_status_lookup rs";
  my $tester_sql_where = "WHERE tr.submission_time>=$self->{_start_timestamp} and tr.submission_time<$self->{_finish_timestamp} AND tr.user_id=u.user_id AND rs.result_status_id=tr.result_status_id";
  my $tester_sql_group_by = "GROUP BY tr.user_id,rs.name";
  my $tester_sql_order_by = "ORDER BY u.irc_nickname DESC, u.email DESC";
  
  if ($self->{_product_id}) {
    $tester_sql_from .= ", testcases t";
    $tester_sql_where .= " AND tr.testcase_id=t.testcase_id AND t.product_id=" . $self->{_product_id};
  }

  if ($self->{_testgroup_id}) {
    $tester_sql_from .= ",  testcase_subgroups tsg, subgroup_testgroups sgtg";
    $tester_sql_where .= " AND tr.testcase_id=tsg.testcase_id AND tsg.subgroup_id=sgtg.subgroup_id AND sgtg.testgroup_id=$self->{_testgroup_id}";
  }
  
  if ($self->{_build_id}) {
    $tester_sql_where .= " AND tr.build_id LIKE '$self->{_build_id}%'";
  }
  
  if ($self->{_branch_id}) {
    $tester_sql_where .= " AND tr.branch_id=$self->{_branch_id}";
  }
  
  if ($self->{_locale}) {
    $tester_sql_where .= " AND tr.locale_abbrev='" . $self->{_locale} . "'";
  }
  
  my $tester_sql = "$tester_sql_select $tester_sql_from $tester_sql_where $tester_sql_group_by $tester_sql_order_by";
  
  my $sth = $self->{_dbh}->prepare($tester_sql);
  $sth->execute();
  
  my $testers;
  while (my @result = $sth->fetchrow_array) {
    $testers->{$result[0]}->{$result[1]} = $result[2];
    $testers->{$result[0]}->{'irc_nickname'} = $result[3];
  }
  $sth->finish;
  
  my @tester_result_statuses;  
  foreach my $key (sort keys %$testers) {
    my $hash_ref;
    $hash_ref->{'email'} = $key;
    $hash_ref->{'irc_nickname'} = $testers->{$key}->{'irc_nickname'};
    $hash_ref->{'pass'} = $testers->{$key}->{'pass'} || 0;
    $hash_ref->{'fail'} = $testers->{$key}->{'fail'} || 0;
    $hash_ref->{'unclear'} = $testers->{$key}->{'unclear'} || 0;
    push @tester_result_statuses, $hash_ref;
  }
  
  return \@tester_result_statuses;
}

#########################################################################
sub getDescription {
  my ($self) = @_;

  return $self->{_description};
}

#########################################################################
sub getStartTimestamp {
  my ($self, $for_display) = @_;

  if ($for_display) {
    return &UnixDate($self->{_start_timestamp},"%Y-%m-%d %H:%M:%S");
  }

  return $self->{_start_timestamp};
}

#########################################################################
sub getFinishTimestamp {
  my ($self, $for_display) = @_;

  if ($for_display) {
    return &UnixDate($self->{_finish_timestamp},"%Y-%m-%d %H:%M:%S");
  }

  return $self->{_finish_timestamp};
}

1;



