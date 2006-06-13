

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

use Litmus::DB::Testresult;
use Memoize;
use Litmus::Error;

our $default_relevance_threshold = 1.0;
our $default_match_limit = 25;

Litmus::DB::Testcase->table('testcases');

Litmus::DB::Testcase->columns(Primary => qw/testcase_id/);
Litmus::DB::Testcase->columns(Essential => qw/summary details enabled community_enabled format_id regression_bug_id product_id/); 
Litmus::DB::Testcase->columns(All => qw/steps expected_results sort_order author_id creation_date last_updated version testrunner_case_id testrunner_case_version/);

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
						      ORDER BY t.sort_order ASC
						     });
__PACKAGE__->set_sql(CommunityEnabledBySubgroup => qq{
							       SELECT t.* 
							       FROM testcases t, testcase_subgroups tsg 
							       WHERE tsg.subgroup_id=? AND tsg.testcase_id=t.testcase_id AND t.enabled=1 AND t.community_enabled=1 
							       ORDER BY t.sort_order ASC
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
					   SELECT testcase_id, summary, MATCH (summary,steps,expected_results) AGAINST (?) AS relevance 
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
sub getDefaultMatchLimit() {
  return $default_match_limit;
}

#########################################################################
sub getDefaultRelevanceThreshold() {
  return $default_relevance_threshold;
}

1;









