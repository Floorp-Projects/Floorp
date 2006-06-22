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

package Litmus::DB::Testresult;

use strict;
use base 'Litmus::DBI';

use Date::Manip;
use Time::Piece;
use Time::Seconds;
use Memoize;

our $_num_results_default = 15;

Litmus::DB::Testresult->table('test_results');

Litmus::DB::Testresult->columns(All => qw/testresult_id testcase_id last_updated submission_time user_id opsys_id branch_id build_id user_agent result_status_id build_type_id machine_name exit_status_id duration_ms talkback_id valid vetted validated_by_user_id vetted_by_user_id validated_timestamp vetted_timestamp locale_abbrev/);

Litmus::DB::Testresult->column_alias("testcase_id", "testcase");
Litmus::DB::Testresult->column_alias("submission_time", "timestamp");
Litmus::DB::Testresult->column_alias("user_id", "user");
Litmus::DB::Testresult->column_alias("opsys_id", "opsys");
Litmus::DB::Testresult->column_alias("branch_id", "branch");
Litmus::DB::Testresult->column_alias("user_agent", "useragent");
Litmus::DB::Testresult->column_alias("result_status_id", "result_status");
Litmus::DB::Testresult->column_alias("build_type_id", "build_type");
Litmus::DB::Testresult->column_alias("exit_status_id", "exit_status");
Litmus::DB::Testresult->column_alias("validity_id", "validity");
Litmus::DB::Testresult->column_alias("vetting_status_id", "vetting_status");
Litmus::DB::Testresult->column_alias("locale_abbrev", "locale");

Litmus::DB::Testresult->has_a(opsys => "Litmus::DB::Opsys");
Litmus::DB::Testresult->has_a(branch => "Litmus::DB::Branch");
Litmus::DB::Testresult->has_a(testcase => "Litmus::DB::Testcase");
Litmus::DB::Testresult->has_a(result_status => "Litmus::DB::ResultStatus");
Litmus::DB::Testresult->has_a(user => "Litmus::DB::User");
Litmus::DB::Testresult->has_a(useragent => "Litmus::UserAgentDetect");
Litmus::DB::Testresult->has_a(build_type => "Litmus::DB::BuildType");
Litmus::DB::Testresult->has_a(exit_status => "Litmus::DB::ExitStatus");
Litmus::DB::Testresult->has_a(locale => "Litmus::DB::Locale");
Litmus::DB::Testresult->has_a(platform => 
                              [ "Litmus::DB::Opsys" => "platform" ]);

Litmus::DB::Testresult->has_many(logs => 
						  ["Litmus::DB::LogTestresult" => 'log_id']);
Litmus::DB::Testresult->has_many(comments => "Litmus::DB::Comment", {order_by => 'comment_id ASC, submission_time ASC'});
Litmus::DB::Testresult->has_many(bugs => "Litmus::DB::Resultbug", {order_by => 'bug_id ASC, submission_time DESC'});

Litmus::DB::Testresult->autoinflate(dates => 'Time::Piece');

Litmus::DB::Testresult->set_sql(DefaultTestResults => qq{
    SELECT tr.testresult_id,tr.testcase_id,t.summary,tr.submission_time AS created,p.name AS platform_name,pr.name as product_name,trsl.name AS result_status,trsl.class_name result_status_class,b.name AS branch_name,tg.name AS test_group_name, tr.locale_abbrev, u.email
    FROM test_results tr, testcases t, platforms p, opsyses o, branches b, products pr, test_result_status_lookup trsl, testgroups tg, subgroups sg, users u, testcase_subgroups tcsg, subgroup_testgroups sgtg
    WHERE tr.testcase_id=t.testcase_id AND tr.opsys_id=o.opsys_id AND o.platform_id=p.platform_id AND tr.branch_id=b.branch_id AND b.product_id=pr.product_id AND tr.result_status_id=trsl.result_status_id AND tcsg.testcase_id=tr.testcase_id AND tcsg.subgroup_id=sg.subgroup_id AND sg.subgroup_id=sgtg.subgroup_id AND sgtg.testgroup_id=tg.testgroup_id AND tr.user_id=u.user_id AND tr.valid=1
    ORDER BY tr.submission_time DESC
    LIMIT $_num_results_default 
});

Litmus::DB::Testresult->set_sql(CommonResults => qq{ 
    SELECT COUNT(tr.testcase_id) AS num_results, tr.testcase_id, t.summary, MAX(tr.submission_time) AS most_recent, MAX(tr.testresult_id) AS max_id 
    FROM test_results tr, testcases t, test_result_status_lookup trsl 
    WHERE tr.testcase_id=t.testcase_id AND tr.result_status_id=trsl.result_status_id AND trsl.class_name=? 
    GROUP BY tr.testcase_id 
    ORDER BY num_results DESC, tr.testresult_id DESC 
    LIMIT 15
    });

Litmus::DB::Testresult->set_sql(Completed => qq{
    SELECT tr.* 
    FROM test_results tr, opsyses o
    WHERE tr.testcase_id=? AND 
        tr.build_id=? AND 
        tr.locale_abbrev=? AND
        tr.opsys_id=o.opsys_id AND
        o.platform_id=?
    ORDER BY tr.submission_time DESC
});

Litmus::DB::Testresult->set_sql(CompletedByUser => qq{
    SELECT tr.* 
    FROM test_results tr, opsyses o
    WHERE tr.testcase_id=? AND 
        tr.build_id=? AND 
        tr.locale_abbrev=? AND
        tr.opsys_id=o.opsys_id AND
        o.platform_id=? AND
        tr.user_id=?
    ORDER BY tr.submission_time DESC
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
    my $criteria = "Default<br/>Ordered by Created<br/>Limit to $_num_results_default results";
    return $criteria, \@rows;
}

#########################################################################
# &getTestResults($\@\@$)
# 
######################################################################### 
sub getTestResults($\@\@$) {
    my ($self,$where_criteria,$order_by_criteria,$limit_value) = @_;
    
    my $select = 'SELECT tr.testresult_id,tr.testcase_id,t.summary,tr.submission_time AS created,p.name AS platform_name,pr.name as product_name,trsl.name AS result_status,trsl.class_name AS result_status_class,b.name AS branch_name,tg.name AS test_group_name, tr.locale_abbrev, u.email';
    
    my $from = 'FROM test_results tr, testcases t, platforms p, opsyses o, branches b, products pr, test_result_status_lookup trsl, testgroups tg, subgroups sg, users u, testcase_subgroups tcsg, subgroup_testgroups sgtg';
    
    my $where = 'WHERE tr.testcase_id=t.testcase_id AND tr.opsys_id=o.opsys_id AND o.platform_id=p.platform_id AND tr.branch_id=b.branch_id AND b.product_id=pr.product_id AND tr.result_status_id=trsl.result_status_id AND tcsg.testcase_id=tr.testcase_id AND tcsg.subgroup_id=sg.subgroup_id AND sg.subgroup_id=sgtg.subgroup_id AND sgtg.testgroup_id=tg.testgroup_id AND tr.user_id=u.user_id AND tr.valid=1';
    
    my $limit = 'LIMIT ';

    foreach my $criterion (@$where_criteria) {
         $criterion->{'value'} =~ s/'/\'/g;
        if ($criterion->{'field'} eq 'branch') {
            $where .= " AND b.name='" . $criterion->{'value'} . "'";
        } elsif ($criterion->{'field'} eq 'locale') {
            $where .= " AND tr.locale_abbrev='" . $criterion->{'value'} . "'";
        } elsif ($criterion->{'field'} eq 'product') {
            $where .= " AND pr.name='" . $criterion->{'value'} . "'";
        } elsif ($criterion->{'field'} eq 'platform') {
            $where .= " AND p.name='" . $criterion->{'value'} . "'";
        } elsif ($criterion->{'field'} eq 'test_group') {
            $where .= " AND tg.name='" . $criterion->{'value'} . "'";
        } elsif ($criterion->{'field'} eq 'testcase_id') {
            $where .= " AND tr.testcase_id='" . $criterion->{'value'} . "'";
        } elsif ($criterion->{'field'} eq 'summary') {
            $where .= ' AND t.summary LIKE \'%%' . $criterion->{'value'} . '%%\'';
        } elsif ($criterion->{'field'} eq 'email') {
            $where .= ' AND u.email LIKE \'%%' . $criterion->{'value'} . '%%\'';
        } elsif ($criterion->{'field'} eq 'result_status') {
            $where .= " AND trsl.class_name='" . $criterion->{'value'} . "'";
        } elsif ($criterion->{'field'} eq 'trusted_only') {            
            if ($from !~ /users u/) {
                $from .= ", users u";
            }
            $where .= " AND u.user_id=tr.user_id AND u.is_admin=1";
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
            ($from,$where) = &_processSearchField($criterion,$from,$where);
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
        } elsif ($criterion->{'field'} eq 'testcase_id') {
            $order_by .= "tr.testcase_id $criterion->{'direction'},";
        } elsif ($criterion->{'field'} eq 'summary') {
            $order_by .= "t.summary $criterion->{'direction'},";
        } elsif ($criterion->{'field'} eq 'result_status') {
            $order_by .= "trsl.class_name $criterion->{'direction'},";
        } elsif ($criterion->{'field'} eq 'branch') {
            $order_by .= "b.name $criterion->{'direction'},";
        } elsif ($criterion->{'field'} eq 'locale') {
            $order_by .= "tr.locale_abbrev $criterion->{'direction'},";
        } elsif ($criterion->{'field'} eq 'email') {
            $order_by .= "u.email $criterion->{'direction'},";
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
#print $sql,"<br/>\n";
    Litmus::DB::Testresult->set_sql(TestResults => qq{
        $sql
        });

    my @rows = $self->search_TestResults();
    
    return \@rows;
}

#########################################################################
# &_processSearchField(\%\$\$)
# 
######################################################################### 
sub _processSearchField(\%) {
    my ($search_field,$from,$where) = @_;
 
    my $table_field = "";
    if ($search_field->{'search_field'} eq 'build_id') {
        $table_field='tr.build_id';
    } elsif ($search_field->{'search_field'} eq 'comments') {
        $table_field='c.comment';        
    } elsif ($search_field->{'search_field'} eq 'locale') {
        $table_field='tr.locale_abbrev';        
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
        $table_field='t.summary';
    } elsif ($search_field->{'search_field'} eq 'test_group') {
        $table_field='tg.name';        
    } elsif ($search_field->{'search_field'} eq 'user_agent') {
        $table_field='tr.user_agent';        
    } else {
        return ($from,$where);
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
      return ($from,$where);
    }

    return ($from,$where);
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
