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

package Litmus::DB::Resultbug;

use strict;
use base 'Litmus::DBI';

use Time::Piece;

Litmus::DB::Resultbug->table('test_result_bugs');

Litmus::DB::Resultbug->columns(Primary => qw/test_result_id bug_id/);
Litmus::DB::Resultbug->columns(All => qw/last_updated submission_time user_id/);

Litmus::DB::Resultbug->column_alias("test_result_id", "test_result");
Litmus::DB::Resultbug->column_alias("test_result_id", "testresult");
Litmus::DB::Resultbug->column_alias("user_id", "user");

Litmus::DB::Resultbug->has_a(test_result => "Litmus::DB::Testresult");
Litmus::DB::Resultbug->has_a(user => "Litmus::DB::User");

Litmus::DB::Resultbug->autoinflate(dates => 'Time::Piece');

1;
