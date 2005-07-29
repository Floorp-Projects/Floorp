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

Litmus::DB::Testresult->table('testresults');

Litmus::DB::Testresult->columns(All => qw/testresultid platform opsys branch buildid useragent result note user testid timestamp/);


Litmus::DB::Testresult->has_a(platform => "Litmus::DB::Platform");
Litmus::DB::Testresult->has_a(opsys => "Litmus::DB::Opsys");
Litmus::DB::Testresult->has_a(branch => "Litmus::DB::Branch");
Litmus::DB::Testresult->has_a(testid => "Litmus::DB::Test");
Litmus::DB::Testresult->has_a(result => "Litmus::DB::Result");
Litmus::DB::Testresult->has_a(user => "Litmus::DB::User");
Litmus::DB::Testresult->has_a(useragent => "Litmus::UserAgentDetect");

Litmus::DB::Testresult->autoinflate(dates => 'Time::Piece');

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