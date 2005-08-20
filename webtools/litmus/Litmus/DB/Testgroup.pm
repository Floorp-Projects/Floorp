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

package Litmus::DB::Testgroup;

use strict;
use base 'Litmus::DBI';

Litmus::DB::Testgroup->table('test_groups');

Litmus::DB::Testgroup->columns(All => qw/testgroup_id product_id name expiration_days/);

Litmus::DB::Testgroup->column_alias("testgroup_id", "testgroupid");
Litmus::DB::Testgroup->column_alias("product_id", "product");
Litmus::DB::Testgroup->column_alias("expiration_days", "expirationdays");

Litmus::DB::Testgroup->has_a(product => "Litmus::DB::Product");

Litmus::DB::Testgroup->has_many(subgroups => "Litmus::DB::Subgroup");

# find the total number of tests completed for the group for 
# a particular platform and optionally just for community enabled tests
sub percentcompleted {
    my $self = shift;
    my $platform = shift;
    my $communityonly = shift;
    my $percentcompleted;
    
    my @subgroups = $self->subgroups();
    my $numemptysubgroups = 0;
    foreach my $cursubgroup (@subgroups) {
        if ($cursubgroup->percentcompleted($platform, $communityonly) eq "N/A") {
            $numemptysubgroups++;
        }
        $percentcompleted += $cursubgroup->percentcompleted($platform, 
                                                            $communityonly);
    }
    
    if (scalar(@subgroups) - $numemptysubgroups == 0) { return "N/A" }
    my $totalpercentage = $percentcompleted/(scalar @subgroups - $numemptysubgroups);
    
    # truncate to a whole number:
    if ($totalpercentage =~ /\./) {
        $totalpercentage =~ /^(\d*)/;
        return $1;
    } else {
        return $totalpercentage;
    }
}

1;