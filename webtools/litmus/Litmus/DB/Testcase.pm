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

package Litmus::DB::Testcase;

use strict;
use base 'Litmus::DBI';

use Date::Manip;
use Litmus::DB::Testresult;
use Memoize;
use Litmus::Error;

our $default_relevance_threshold = 1.0;
our $default_match_limit = 25;
our $default_num_days = 7;

Litmus::DB::Testcase->table('testcases');

Litmus::DB::Testcase->columns(Primary => qw/testcase_id/);
Litmus::DB::Testcase->columns(Essential => qw/summary details enabled community_enabled format_id regression_bug_id product_id steps expected_results author_id creation_date last_updated version testrunner_case_id testrunner_case_version/);

Litmus::DB::Testcase->column_alias("testcase_id", "testid");
Litmus::DB::Testcase->column_alias("testcase_id", "test_id");
Litmus::DB::Testcase->column_alias("testcase_id", "id");
Litmus::DB::Testcase->column_alias("community_enabled", "communityenabled");
Litmus::DB::Testcase->column_alias("format_id", "format");
Litmus::DB::Testcase->column_alias("author_id", "author");
Litmus::DB::Testcase->column_alias("product_id", "product");

Litmus::DB::Testcase->has_a("format" => "Litmus::DB::Format");
Litmus::DB::Testcase->has_a("author" => "Litmus::DB::User");
Litmus::DB::Testcase->has_a("product" => "Litmus::DB::Product");

__PACKAGE__->set_sql(EnabledBySubgroup => qq{
				             SELECT t.* 
					     FROM testcases t, testcase_subgroups tsg
					     WHERE tsg.subgroup_id=? AND tsg.testcase_id=t.testcase_id AND t.enabled=1 
					     ORDER BY tsg.sort_order ASC
});

__PACKAGE__->set_sql(CommunityEnabledBySubgroup => qq{
                                                      SELECT t.* 
                                                      FROM testcases t, testcase_subgroups tsg
						      WHERE tsg.subgroup_id=? AND tsg.testcase_id=t.testcase_id AND t.enabled=1 AND t.community_enabled=1
						      ORDER BY tsg.sort_order ASC, t.testcase_id ASC
});

Litmus::DB::Testcase->has_many(test_results => "Litmus::DB::Testresult", {order_by => 'submission_time DESC'});

#########################################################################
# is_completed($$$$$)
#
# Check whether we have test results for the current test that correspond
# to the provided platform, build_id, and user(optional).
#########################################################################
memoize('is_completed');
sub is_completed {
  my $self = shift;
  my $platform = shift;
  my $build_id = shift;
  my $locale = shift;
  my $user = shift;        # optional

  my @results;
  if ($user) {
    @results = Litmus::DB::Testresult->search_CompletedByUser(
                                                              $self->{'testcase_id'},
                                                              $build_id,
                                                              $locale->{'abbrev'},
                                                              $platform->{'platform_id'},
                                                              $user->{'user_id'},
                                                             );
  } else {
    @results = Litmus::DB::Testresult->search_Completed(
                                                        $self->{'testcase_id'},
                                                        $build_id,
                                                        $locale->{'abbrev'},
                                                        $platform->{'platform_id'},
                                                       );
  }

  return @results;
}

#########################################################################
sub getFullTextMatches() {
  my $self = shift;
  my $text_snippet = shift;
  my $match_limit = shift;
  my $relevance_threshold = shift;
  
  if (!$match_limit) {
    $match_limit = $default_match_limit;
  }
  if (!$relevance_threshold) {
    $relevance_threshold = $default_relevance_threshold
  }
  
  __PACKAGE__->set_sql(FullTextMatches => qq{
                                             SELECT testcase_id, summary, creation_date, last_updated, MATCH (summary,steps,expected_results) AGAINST (?) AS relevance
					     FROM testcases
					     WHERE MATCH (summary,steps,expected_results) AGAINST (?) HAVING relevance > ?
					     ORDER BY relevance DESC, summary ASC
                                             LIMIT $match_limit
});
  
  
  return $self->search_FullTextMatches(
                                       $text_snippet,
                                       $text_snippet,
                                       $relevance_threshold
                                      );
}

#########################################################################
sub getNewTestcases() {
  my $self = shift;
  my $num_days = shift;
  my $match_limit = shift;
  
  if (!$num_days) {
    $num_days = $default_num_days;
  }
  
  if (!$match_limit) {
    $match_limit = $default_match_limit;
  }
  
  __PACKAGE__->set_sql(NewTestcases => qq{
				          SELECT testcase_id, summary, creation_date, last_updated
                                          FROM testcases
                                          WHERE creation_date>=?
                                          ORDER BY creation_date DESC
                                          LIMIT $match_limit
});
  
  my $err;
  my $new_datestamp=&UnixDate(DateCalc("now","- $num_days days"),"%q");
  return $self->search_NewTestcases($new_datestamp);
}

#########################################################################
sub getRecentlyUpdated() {
  my $self = shift;
  my $num_days = shift;
  my $match_limit = shift;
  
  if (!$num_days) {
    $num_days = $default_num_days;
  }
  
  if (!$match_limit) {
    $match_limit = $default_match_limit;
  }
  
  __PACKAGE__->set_sql(RecentlyUpdated => qq{
                                             SELECT testcase_id, summary, creation_date, last_updated
                                             FROM testcases
                                             WHERE last_updated>=? AND last_updated>creation_date
                                             ORDER BY last_updated DESC
                                             LIMIT $match_limit
});
  
  my $err;
  my $new_datestamp=&UnixDate(DateCalc("now","- $num_days days"),"%q");
  return $self->search_RecentlyUpdated($new_datestamp);
}

#########################################################################
sub getDefaultMatchLimit() {
  return $default_match_limit;
}

#########################################################################
sub getDefaultRelevanceThreshold() {
  return $default_relevance_threshold;
}

#########################################################################
sub getDefaultNumDays() {
  return $default_num_days;
}

#########################################################################
sub clone() {
  my $self = shift;
  
  my $new_testcase = $self->copy;
  if (!$new_testcase) {
    return undef;
  }
  
  # Update dates to now.
  my $now = &UnixDate("today","%q");
  $new_testcase->creation_date($now);
  $new_testcase->last_updated($now);
  $new_testcase->update();
  
  # Propagate subgroup membership;
  my $dbh = __PACKAGE__->db_Main();
  my $sql = "INSERT INTO testcase_subgroups (testcase_id,subgroup_id,sort_order) SELECT ?,subgroup_id,sort_order FROM testcase_subgroups WHERE testcase_id=?";
  
  my $rows = $dbh->do($sql,
		      undef,
		      $new_testcase->testcase_id,
		      $self->testcase_id
		     );
  if (! $rows) {
    # XXX: Do we need to throw a warning here?
    # What happens when we clone a testcase that doesn't belong to
    # any subgroups?
  }
  
  $sql = "INSERT INTO related_testcases (testcase_id, related_testcase_id) VALUES (?,?)";
  $rows = $dbh->do($sql, 
                   undef,
                   $self->testcase_id,
                   $new_testcase->testcase_id
                  );
  if (! $rows) {
    # XXX: Do we need to throw a warning here?
  }
  
  return $new_testcase;
}

#########################################################################
sub delete_from_subgroups() {
  my $self = shift;
  
  my $dbh = __PACKAGE__->db_Main();
  my $sql = "DELETE from testcase_subgroups WHERE testcase_id=?";
  return $dbh->do($sql,
                  undef,
                  $self->testcase_id
                 );
}

#########################################################################
sub delete_from_subgroup() {
  my $self = shift;
  my $subgroup_id = shift;  

  my $dbh = __PACKAGE__->db_Main();
  my $sql = "DELETE from testcase_subgroups WHERE testcase_id=? AND subgroup_id=?";
  return $dbh->do($sql,
                  undef,
                  $self->testcase_id,
                  $subgroup_id
                 );
}

#########################################################################
sub delete_from_related() {
  my $self = shift;
  
  my $dbh = __PACKAGE__->db_Main();
  my $sql = "DELETE from related_testcases WHERE testcase_id=? OR related_testcase_id=?";
  return $dbh->do($sql,
                  undef,
                  $self->testcase_id,
                  $self->testcase_id
                 );
}

#########################################################################
sub delete_with_refs() {
  my $self = shift;
  $self->delete_from_subgroups();
  $self->delete_from_related();
  return $self->delete;
}

#########################################################################
sub update_subgroups() {
  my $self = shift;
  my $new_subgroup_ids = shift;
  
  if (scalar @$new_subgroup_ids) {
    # Failing to delete subgroups is _not_ fatal when adding a new testcase.
    my $rv = $self->delete_from_subgroups();
    my $dbh = __PACKAGE__->db_Main();
    my $sql = "INSERT INTO testcase_subgroups (testcase_id,subgroup_id,sort_order) VALUES (?,?,1)";
    foreach my $new_subgroup_id (@$new_subgroup_ids) {
      my $rows = $dbh->do($sql, 
			  undef,
			  $self->testcase_id,
			  $new_subgroup_id
			 );
    }
  }
}

#########################################################################
sub update_subgroup() {
  my $self = shift;
  my $subgroup_id = shift;
  my $sort_order = shift;

  # Sort order defaults to 1.
  if (!$sort_order) {
    $sort_order = 1;
  }
  
  my $rv = $self->delete_from_subgroup($subgroup_id);
  my $dbh = __PACKAGE__->db_Main();
  my $sql = "INSERT INTO testcase_subgroups (testcase_id,subgroup_id,sort_order) VALUES (?,?,?)";
  return $dbh->do($sql, 
                  undef,
                  $self->testcase_id,
                  $subgroup_id,
                  $sort_order
                 );
}

1;









