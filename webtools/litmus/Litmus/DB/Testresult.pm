# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is Litmus.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Zach Lipton <zach@zachlipton.com>

package Litmus::DB::Testresult;

use strict;
use base 'Litmus::DBI';

use Time::Piece;
use Time::Seconds;
use Memoize;

Litmus::DB::Testresult->table('test_results');

Litmus::DB::Testresult->columns(All => qw/testresult_id test_id last_updated submission_time user_id platform_id opsys_id branch_id buildid user_agent result_id log_id/);

Litmus::DB::Testresult->column_alias("testresult_id", "testresultid");
Litmus::DB::Testresult->column_alias("test_id", "testid");
Litmus::DB::Testresult->column_alias("submission_time", "timestamp");
Litmus::DB::Testresult->column_alias("user_id", "user");
Litmus::DB::Testresult->column_alias("platform_id", "platform");
Litmus::DB::Testresult->column_alias("opsys_id", "opsys");
Litmus::DB::Testresult->column_alias("branch_id", "branch");
Litmus::DB::Testresult->column_alias("user_agent", "useragent");
Litmus::DB::Testresult->column_alias("result_id", "result");
Litmus::DB::Testresult->column_alias("log_id", "log");


Litmus::DB::Testresult->has_a(platform => "Litmus::DB::Platform");
Litmus::DB::Testresult->has_a(opsys => "Litmus::DB::Opsys");
Litmus::DB::Testresult->has_a(branch => "Litmus::DB::Branch");
Litmus::DB::Testresult->has_a(testid => "Litmus::DB::Test");
Litmus::DB::Testresult->has_a(result => "Litmus::DB::Result");
Litmus::DB::Testresult->has_a(user => "Litmus::DB::User");
Litmus::DB::Testresult->has_a(useragent => "Litmus::UserAgentDetect");
Litmus::DB::Testresult->has_a("log" => "Litmus::DB::Log");

Litmus::DB::Testresult->has_many(comments => "Litmus::DB::Comment", {order_by => 'submission_time'});
Litmus::DB::Testresult->has_many(bugs => "Litmus::DB::Resultbug", {order_by => 'submission_time DESC'});

Litmus::DB::Testresult->autoinflate(dates => 'Time::Piece');

# for historical reasons, note() is a shorthand way of saying "the text of the first 
# comment on this result if that comment was submitted by the result submitter"
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

# get the age of this result and return it as a Time::Seconds object
memoize('age', NORMALIZER => sub {my $a=shift; return $a->testresultid()});
sub age {
    my $self = shift;
    
    my $now = localtime;
    my $timediff =  $now - $self->timestamp();
    return $timediff;
}

# is this test result from a trusted user?
sub istrusted {
    my $self = shift; 
    
    if ($self->user()->istrusted()) {
        return 1;
    } else {
        return 0;
    }
}

1;