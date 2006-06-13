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

package Litmus::DB::TestcaseSubgroup;

use strict;
use base 'Litmus::DBI';

Litmus::DB::TestcaseSubgroup->table('testcase_subgroups');

Litmus::DB::TestcaseSubgroup->columns(All => qw/testcase_id subgroup_id/);

Litmus::DB::TestcaseSubgroup->has_a(testcase_id => "Litmus::DB::Testcase");
Litmus::DB::TestcaseSubgroup->has_a(subgroup_id => "Litmus::DB::Subgroup");

Litmus::DB::TestcaseSubgroup->column_alias("testcase_id", "testcase");
Litmus::DB::TestcaseSubgroup->column_alias("testcase_id", "test");
Litmus::DB::TestcaseSubgroup->column_alias("subgroup_id", "subgroup");

1;
