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

package Litmus::DB::Test;

use strict;
use base 'Litmus::DBI';

use Litmus::DB::Testresult;
use Memoize;
use Litmus::Error;

Litmus::DB::Test->table('tests');

Litmus::DB::Test->columns(Primary => qw/test_id/);
Litmus::DB::Test->columns(Essential => qw/subgroup_id summary details status_id community_enabled format_id regression_bug_id/); 
Litmus::DB::Test->columns(All => qw/steps expected_results/);

Litmus::DB::Test->column_alias("test_id", "testid");
Litmus::DB::Test->column_alias("subgroup_id", "subgroup");
Litmus::DB::Test->column_alias("status_id", "status");
Litmus::DB::Test->column_alias("community_enabled", "communityenabled");
Litmus::DB::Test->column_alias("format_id", "format");

Litmus::DB::Test->has_a(subgroup => "Litmus::DB::Subgroup");
Litmus::DB::Test->has_a(status => "Litmus::DB::Status");
Litmus::DB::Test->has_a("format" => "Litmus::DB::Format");

Litmus::DB::Test->has_many(testresults => "Litmus::DB::Testresult", {order_by => 'submission_time DESC'});

#########################################################################
# does the test have at least one recent result?
# optionally, just check for a particular platform.
#########################################################################
memoize('isrecent');
sub isrecent {
  my $self = shift;
  my $platform = shift;
  
  my %restrictor;
  if ($platform) { $restrictor{platform} = $platform }
  
  my @results = $self->testresults(%restrictor);
  foreach my $curresult (@results) {
    if ($curresult->isrecent()) {
      return 1;
    }
  }
  return 0;
}

#########################################################################
# is_completed($$$$)
#
# Check whether we have test results for the current test that correspond
# to the provided platform, build_id, and user(optional).
#########################################################################
memoize('is_completed');
sub is_completed {
  my $self = shift;
  my $platform = shift;
  my $build_id = shift;
  my $user = shift;        # optional

  my @results;
  if ($user) {
    @results = $self->testresults(
                                  platform => $platform,
                                  buildid => $build_id,
                                  user => $user,
                                 );
  } else {
    @results = $self->testresults(
                                  platform => $platform,
                                  buildid => $build_id,
                                 );
  }
  
  return scalar @results;  
}

#########################################################################
# You might think that getting the state of a test for a particular platform 
# would be pretty easy. In reality, it's more of an art then a science, since 
# we get to consider all the test results submitted for a particular test, 
# their age, whether the result is from a trusted user, and other fun things. 
#
# Or in other words: "Heuristics are bug ridden by definition. If they didn't 
# have bugs, then they'd be algorithms."
#
# XXX: Rewrite all this as an SQL query so it doesn't take so long. 
#
# YYY: 'state' is even less simple than you might think, and should be
# based per-branch and make note of the most recent build ID per 
# platform.
#########################################################################
memoize('state');
sub state {
    my $self = shift;
    my $platform = shift;
    
    # XXX: if the test is automated, just return the most recent state
    
    my %statecounts; 
    
    # first get all results for this test for this platform:
    my @results = $self->testresults(platform => $platform);
    foreach my $curresult (@results) {
        if (! $curresult->isrecent($platform)) {
            # only consider recent results
            next; 
        } 
        # we weight the result based on its age and if it is confirmed:
        # first figure out how old the result is as a proportion of the 
        # expiration time for the group using a grannuler definition of a day:
        my $adjustedage;
        if ($curresult->age()->days() < 1.5)  {
            $adjustedage = 1;
        } elsif ($curresult->age()->days() < 2.5) {
            $adjustedage = 1.8;
        } else {
            $adjustedage = $curresult->age()->days();
        }
        
        my $ageproportion = $self->subgroup()->testgroup()->expirationdays()/$adjustedage;
        my $weight = $ageproportion;
        
        # give an additional weighting of 2 points to confirmed results:
        if ($curresult->istrusted()) {
            $weight += 2;
        }
        
        $statecounts{$curresult->result()} += $weight;
    }
    
    # now that we have the weighted counts for each possible state, we 
    # calculate the magic number for this test. In other words, the 
    # result spread that we require in order to have confidence in our 
    # result. If the spread between two states is within the magic 
    # number, we just return 0 and the test should be considered unrun
    # since we have no confidence in our result. 
    my $magicnumber = 2; # ok we don't really calculate it. We should though...
    
    foreach my $outer (keys(%statecounts)) {
        foreach my $inner (keys(%statecounts)) {
            if ($outer eq $inner) {
                next;
            }
            if (abs($statecounts{$inner} - $statecounts{$outer}) < $magicnumber) {
                return 0;
            }
        }
    }
    
    # now we just find the state with the greatest value and return it:
    my $maxkey;
    foreach my $cur (keys(%statecounts)) {
        unless ($maxkey) {$maxkey = $cur}
        if ($statecounts{$cur} > $statecounts{$maxkey}) {
            $maxkey = $cur;
        }
    }
    
    return Litmus::DB::Result->retrieve($maxkey);
}

#########################################################################
# calculate the percent of the time this test has been in existance that it
# has had a particular state (default state is the current one)
#########################################################################
sub percentinstate {
    my $self = shift;
    my $state = shift || $self->state();
    
    
}

#########################################################################
# find the number of recent results for this test
#########################################################################
memoize('num_recent_results');
sub num_recent_results {
    my $self = shift;
    
    my $count;
    foreach my $curresult ($self->testresults()) {
        if ($curresult->isrecent()) {
            $count++;
        }
    }
    return $count;
}

#########################################################################
# these are just convenience functions since they are pretty common needs 
# and templates would be pretty verbose without them:
#########################################################################
sub product {
    my $self = shift;
    return $self->testgroup()->product();
}

#########################################################################
#########################################################################
sub testgroup {
    my $self = shift;
    return $self->subgroup()->testgroup();
}

1;









