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

package Litmus::DB::Testresult;

use strict;
use base 'Litmus::DBI';

use Date::Manip;
use Time::Piece;
use Time::Seconds;
use Memoize;

our $_num_results_default = 15;

Litmus::DB::Testresult->table('test_results');

Litmus::DB::Testresult->columns(All => qw/testresult_id test_id last_updated submission_time user_id platform_id opsys_id branch_id buildid user_agent result_id build_type_id machine_name exit_status_id duration_ms talkback_id validity_id vetting_status_id/);

Litmus::DB::Testresult->column_alias("testresult_id", "testresultid");
Litmus::DB::Testresult->column_alias("test_id", "testid");
Litmus::DB::Testresult->column_alias("submission_time", "timestamp");
Litmus::DB::Testresult->column_alias("user_id", "user");
Litmus::DB::Testresult->column_alias("platform_id", "platform");
Litmus::DB::Testresult->column_alias("opsys_id", "opsys");
Litmus::DB::Testresult->column_alias("branch_id", "branch");
Litmus::DB::Testresult->column_alias("user_agent", "useragent");
Litmus::DB::Testresult->column_alias("result_id", "result");
Litmus::DB::Testresult->column_alias("build_type_id", "build_type");
Litmus::DB::Testresult->column_alias("exit_status_id", "exit_status");
Litmus::DB::Testresult->column_alias("validity_id", "validity");
Litmus::DB::Testresult->column_alias("vetting_status_id", "vetting_status");

Litmus::DB::Testresult->has_a(platform => "Litmus::DB::Platform");
Litmus::DB::Testresult->has_a(opsys => "Litmus::DB::Opsys");
Litmus::DB::Testresult->has_a(branch => "Litmus::DB::Branch");
Litmus::DB::Testresult->has_a(testid => "Litmus::DB::Test");
Litmus::DB::Testresult->has_a(result => "Litmus::DB::Result");
Litmus::DB::Testresult->has_a(user => "Litmus::DB::User");
Litmus::DB::Testresult->has_a(useragent => "Litmus::UserAgentDetect");
Litmus::DB::Testresult->has_a(build_type => "Litmus::DB::BuildType");
Litmus::DB::Testresult->has_a(exit_status => "Litmus::DB::ExitStatus");
Litmus::DB::Testresult->has_a(validity => "Litmus::DB::Validity");
Litmus::DB::Testresult->has_a(vetting_status => "Litmus::DB::VettingStatus");

Litmus::DB::Testresult->has_many("logs" => "Litmus::DB::Log", {order_by => 'submission_time'});
Litmus::DB::Testresult->has_many(comments => "Litmus::DB::Comment", {order_by => 'comment_id ASC, submission_time ASC'});
Litmus::DB::Testresult->has_many(bugs => "Litmus::DB::Resultbug", {order_by => 'bug_id ASC, submission_time DESC'});

Litmus::DB::Testresult->autoinflate(dates => 'Time::Piece');

Litmus::DB::Testresult->set_sql(DefaultTestResults => qq{
    SELECT tr.testresult_id,tr.test_id,t.summary,tr.submission_time AS created,p.name AS platform_name,pr.name as product_name,trsl.name AS result_status,trsl.class_name result_status_class,b.name AS branch_name,tg.name AS test_group_name
    FROM test_results tr, tests t, platforms p, opsyses o, branches b, products
pr, test_result_status_lookup trsl, test_groups tg, subgroups sg
    WHERE tr.test_id=t.test_id AND tr.platform_id=p.platform_id AND tr.opsys_id=o.opsys_id AND tr.branch_id=b.branch_id AND b.product_id=pr.product_id AND tr.result_id=trsl.result_status_id AND t.subgroup_id=sg.subgroup_id AND sg.testgroup_id=tg.testgroup_id 
    ORDER BY tr.submission_time DESC
    LIMIT $_num_results_default 
});

Litmus::DB::Testresult->set_sql(CommonResults => qq{ 
    SELECT COUNT(tr.test_id) AS num_results, tr.test_id, t.summary, MAX(tr.submission_time) AS most_recent, MAX(tr.testresult_id) AS max_id 
    FROM test_results tr, tests t, test_result_status_lookup trsl 
    WHERE tr.test_id=t.test_id AND tr.result_id=trsl.result_status_id AND trsl.class_name=? 
    GROUP BY tr.test_id 
    ORDER BY num_results DESC, tr.testresult_id DESC 
    LIMIT 15
    });

#########################################################################
# for historical reasons, note() is a shorthand way of saying "the text of 
# the first comment on this result if that comment was submitted by the 
# result submitter"
#########################################################################
sub note {
    my $self = shift;
    
    my @comments = $self->comments();
    
    if (@comments && $comments[0] &&
        $comments[0]->user() == $self->user()) {
        return $comments[0]->comment();
    } else {
        return undef;
    }
}

#########################################################################
# is this test result recent?
memoize('isrecent', NORMALIZER => sub {my $a=shift; return $a->testresultid()});
sub isrecent {
  my $self = shift;

  my $age = $self->age();
  
  # get the number of days a test result is valid for this group:
  my $expdays = $self->testid()->subgroup()->testgroup()->expirationdays();
  
  if ($age->days() < $expdays) {
    return 1;
  } else {
    return 0;
  }
}

#########################################################################
# get the age of this result and return it as a Time::Seconds object
memoize('age', NORMALIZER => sub {my $a=shift; return $a->testresultid()});
sub age {
  my $self = shift;
  
  my $now = localtime;  
  my $timediff =  $now - $self->timestamp();
  return $timediff;
}

#########################################################################
# is this test result from a trusted user?
#########################################################################
sub istrusted {
  my $self = shift; 
  
   if ($self->user()->istrusted()) {
    return 1;
  } else {
    return 0;
  }
}

#########################################################################
# &getDefaultTestResults($)
#
#########################################################################
sub getDefaultTestResults($) {
    my $self = shift;
    my @rows = $self->search_DefaultTestResults();
    my $criteria = "Default<br/>Ordered by Created<br>Limit to $_num_results_default results";
    return $criteria, \@rows;
}

#########################################################################
# &getTestResults($\@\@$)
# 
######################################################################### 
sub getTestResults($\@\@$) {
    my ($self,$where_criteria,$order_by_criteria,$limit_value) = @_;
    
    my $select = 'SELECT tr.testresult_id,tr.test_id,t.summary,tr.submission_time AS created,p.name AS platform_name,pr.name as product_name,trsl.name AS result_status,trsl.class_name AS result_status_class,b.name AS branch_name,tg.name AS test_group_name';
    
    my $from = 'FROM test_results tr, tests t, platforms p, opsyses o, branches b, products pr, test_result_status_lookup trsl, test_groups tg, subgroups sg';
    
    my $where = 'WHERE tr.test_id=t.test_id AND tr.platform_id=p.platform_id AND tr.opsys_id=o.opsys_id AND tr.branch_id=b.branch_id AND b.product_id=pr.product_id AND tr.result_id=trsl.result_status_id AND t.subgroup_id=sg.subgroup_id AND sg.testgroup_id=tg.testgroup_id';
    
    my $limit = 'LIMIT ';

    foreach my $criterion (@$where_criteria) {
        if ($criterion->{'field'} eq 'branch') {
            $where .= " AND b.name='" . $criterion->{'value'} . "'";
        } elsif ($criterion->{'field'} eq 'product') {
            $where .= " AND pr.name='" . $criterion->{'value'} . "'";
        } elsif ($criterion->{'field'} eq 'platform') {
            $where .= " AND p.name='" . $criterion->{'value'} . "'";
        } elsif ($criterion->{'field'} eq 'test_group') {
            $where .= " AND tg.name='" . $criterion->{'value'} . "'";
        } elsif ($criterion->{'field'} eq 'test_id') {
            $where .= " AND tr.test_id='" . $criterion->{'value'} . "'";
        } elsif ($criterion->{'field'} eq 'summary') {
            $where .= ' AND t.summary LIKE \'%%' . $criterion->{'value'} . '%%\'';
        } elsif ($criterion->{'field'} eq 'result_status') {
            $where .= " AND trsl.class_name='" . $criterion->{'value'} . "'";
        } elsif ($criterion->{'field'} eq 'trusted_only') {            
            $from .= ", users u";
            $where .= " AND u.user_id=tr.user_id AND u.is_trusted=1";
        } elsif ($criterion->{'field'} eq 'start_date') {
            my $start_timestamp = &Date::Manip::UnixDate(&Date::Manip::ParseDateString($criterion->{'value'}),"%q");
            if ($start_timestamp !~ /^\d\d\d\d\d\d\d\d\d\d\d\d\d\d$/) {
                print STDERR "Unable to parse a valid start date from '$criterion->{'value'},' ignoring.\n";
            } else {
                $where .= " AND tr.submission_time>=$start_timestamp";
            }
        } elsif ($criterion->{'field'} eq 'end_date') {
            my $end_timestamp = &Date::Manip::UnixDate(&Date::Manip::ParseDateString($criterion->{'value'}),"%q");
            if ($end_timestamp !~ /^\d\d\d\d\d\d\d\d\d\d\d\d\d\d$/) {
                print STDERR "Unable to parse a valid end date from '$criterion->{'value'},' ignoring.\n";
            } else {
                $where .= " AND tr.submission_time<=$end_timestamp";
            }
        } elsif ($criterion->{'field'} eq 'timespan') {
            next if ($criterion->{'value'} eq 'all');
            my $day_delta = $criterion->{'value'};
            my $err;
            my $timestamp = 
                &Date::Manip::UnixDate(&Date::Manip::DateCalc("now",
                                                              "$day_delta days",
                                                              \$err),
                                       "%q");
            $where .= " AND tr.submission_time>=$timestamp";

        } elsif ($criterion->{'field'} eq 'search_field') {
            my $rv = &_processSearchField($criterion,\$from,\$where);
        } else {
            # Skip unknown field
        }
    }

    my $order_by = 'ORDER BY ';
    foreach my $criterion (@$order_by_criteria) {
        # Skip empty fields.
        next if (!$criterion or !$criterion->{'field'});

        if ($criterion->{'field'} eq 'created') {
            $order_by .= "tr.submission_time $criterion->{'direction'},";
        } elsif ($criterion->{'field'} eq 'product') {
            $order_by .= "pr.name $criterion->{'direction'},";
        } elsif ($criterion->{'field'} eq 'platform') {
            $order_by .= "p.name $criterion->{'direction'},";
        } elsif ($criterion->{'field'} eq 'test_group') {
            $order_by .= "tg.name $criterion->{'direction'},";
        } elsif ($criterion->{'field'} eq 'test_id') {
            $order_by .= "tr.test_id $criterion->{'direction'},";
        } elsif ($criterion->{'field'} eq 'summary') {
            $order_by .= "t.summary $criterion->{'direction'},";
        } elsif ($criterion->{'field'} eq 'result_status') {
            $order_by .= "trsl.class_name $criterion->{'direction'},";
        } elsif ($criterion->{'field'} eq 'branch') {
            $order_by .= "b.name $criterion->{'direction'},";
        } else {
            # Skip unknown field
        }
    }
    if ($order_by eq 'ORDER BY ') {
        $order_by .= 'tr.submission_time DESC';
    } else {
        chop($order_by);
    }

    if ($limit_value and $limit_value ne '') {
        $limit .= "$limit_value";
    } else {
        $limit .= "$_num_results_default";
    }
    
    my $sql = "$select $from $where $order_by $limit";

    Litmus::DB::Testresult->set_sql(TestResults => qq{
        $sql
        });

    my @rows = $self->search_TestResults();
    
    return \@rows;
}

#########################################################################
# &_processSearchField(\%)
# 
######################################################################### 
sub _processSearchField(\%) {
    my ($search_field,$from,$where) = @_;
    
    my $table_field = "";
    if ($search_field->{'search_field'} eq 'buildid') {
        $table_field='tr.build_id';
    } elsif ($search_field->{'search_field'} eq 'comments') {
        $table_field='c.comment';        
    } elsif ($search_field->{'search_field'} eq 'opsys') {
        $table_field='o.name';
    } elsif ($search_field->{'search_field'} eq 'platform') {
        $table_field='p.name';
    } elsif ($search_field->{'search_field'} eq 'product') {
        $table_field='pr.name';
    } elsif ($search_field->{'search_field'} eq 'result_status') {
        $table_field='trsl.name';        
    } elsif ($search_field->{'search_field'} eq 'subgroup') {
        $table_field='sg.name';        
    } elsif ($search_field->{'search_field'} eq 'email') {
        if ($from !~ /users u/) {
            $from .= ", users u";
            $where .= " AND tr.user_id=u.user_id";
        }
        $table_field='u.email';        
    } elsif ($search_field->{'search_field'} eq 'summary') {
        $table_field='t.name';
    } elsif ($search_field->{'search_field'} eq 'test_group') {
        $table_field='tg.name';        
    } elsif ($search_field->{'search_field'} eq 'test_status') {
        $table_field='tsl.name';        
    } elsif ($search_field->{'search_field'} eq 'user_agent') {
        $table_field='tr.user_agent';        
    } else {
        return undef;
    }

    if ($search_field->{'match_criteria'} eq 'contains_all' or
        $search_field->{'match_criteria'} eq 'contains_any' or
        $search_field->{'match_criteria'} eq 'not_contain_any') {
        
        my $join = "";
        if ($search_field->{'match_criteria'} eq 'contains_all') {
            $join = 'AND';
        } else {
            $join = 'OR';
        }

        $search_field->{'value'} =~ s/\\//g;
        my @words = split(/ /,$search_field->{'value'});
        if ($search_field->{'match_criteria'} eq 'not_contain_any') {
            $where .= " AND NOT (";
        } else {
            $where .= " AND (";
        }
        my $first_pass = 1;
        foreach my $word (@words) {
            if ( $first_pass ) {
                $where .= "UPPER($table_field) LIKE UPPER('%%" . $word . "%%')";
                $first_pass = 0;
            } else {
                $where .= " $join UPPER($table_field) LIKE UPPER('%%" . $word . "%%')";
            }
        }
        $where .= ")";        
    } elsif ($search_field->{'match_criteria'} eq 'contains') {
        $where .= " AND UPPER($table_field) LIKE UPPER('%%" . $search_field->{'value'} . "%%')";
    } elsif ($search_field->{'match_criteria'} eq 'contains_case') {
        $where .= " AND $table_field LIKE '%%" . $search_field->{'value'} . "%%'";
    } elsif ($search_field->{'match_criteria'} eq 'not_contain') {
        $where .= " AND UPPER($table_field) NOT LIKE UPPER('%%" . $search_field->{'value'} . "%%')";
    } elsif ($search_field->{'match_criteria'} eq 'regexp') {
        $where .= " AND $table_field REGEXP '" . $search_field->{'value'} . "'";        
    } elsif ($search_field->{'match_criteria'} eq 'not_regexp') {        
        $where .= " AND $table_field NOT REGEXP '" . $search_field->{'value'} . "'";        
    } else {
        # Ignore unknown match criteria.
        return undef;
    }

    return 0;
}

#########################################################################
#########################################################################
sub getCommonResults($$) {
    my ($self,$status,$limit_value) = @_;
    
    if (!$status) {
      return undef;
    }
    
    if (!$limit_value) {
        $limit_value = $_num_results_default;
    }

    my @rows = $self->search_CommonResults($status);
    return \@rows;    
}

1;
