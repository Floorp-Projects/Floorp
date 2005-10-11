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

package Litmus::DB::Comment;

use strict;
use base 'Litmus::DBI';

Litmus::DB::Comment->table('test_result_comments');

Litmus::DB::Comment->columns(All => qw/comment_id test_result_id last_updated submission_time user_id comment/);

Litmus::DB::Comment->column_alias("comment_id", "commentid");
Litmus::DB::Comment->column_alias("test_result_id", "testresult");
Litmus::DB::Comment->column_alias("user_id", "user");

Litmus::DB::Comment->has_a(test_result_id => "Litmus::DB::Testresult");
Litmus::DB::Comment->has_a(user => "Litmus::DB::User");

Litmus::DB::Testresult->autoinflate(dates => 'Time::Piece');

1;
