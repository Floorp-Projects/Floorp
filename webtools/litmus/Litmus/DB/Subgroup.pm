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

# find the percentage of testing completed for a particular platform in
# this subgroup, optionally restricting to community enabled tests only
sub percentcompleted {
    my $self = shift;
    my $platform = shift;
    my $communityonly = shift;
    
    my @tests;
    if (! $communityonly) {    
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
    my $numcompleted = 0;
    foreach my $curtest (@tests) {
        if ($curtest->iscompleted($platform)) {
            $numcompleted++;
        }
    }

    my $result = ($numcompleted/scalar @tests) * 100;
    unless ($result) {                   
        return "0";
    }
    # truncate to a whole number:
    if ($result =~ /\./) {
        $result =~ /^(\d*)/;
        return $1;
    } else {
        return $result;
    }
}


1;