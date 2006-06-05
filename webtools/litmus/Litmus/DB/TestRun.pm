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

package Litmus::DB::TestRun;

use strict;
use base 'Litmus::DBI';

use Time::Piece;
use Time::Seconds;

Litmus::DB::TestRun->table('test_runs');

Litmus::DB::TestRun->columns(All => qw/test_run_id testgroup_id name description start_timestamp finish_timestamp enabled/);

Litmus::DB::TestRun->column_alias("testgroup_id", "testgroup");

Litmus::DB::TestRun->has_a(testgroup => "Litmus::DB::Testgroup");
Litmus::DB::TestRun->has_many(subgroups => 
                                [ "Litmus::DB::SubgroupTestgroup" => "testgroup" ]);

Litmus::DB::TestRun->autoinflate(dates => 'Time::Piece');

1;








