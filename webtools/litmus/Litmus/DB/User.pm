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

package Litmus::DB::User;

use strict;
use base 'Litmus::DBI';

Litmus::DB::User->table('users');

Litmus::DB::User->columns(All => qw/user_id bugzilla_uid email password realname 
	irc_nickname disabled is_admin/);

Litmus::DB::User->column_alias("user_id", "userid");
Litmus::DB::User->column_alias("is_trusted", "istrusted");
Litmus::DB::User->column_alias("is_admin", "is_trusted");

Litmus::DB::User->has_many(testresults => "Litmus::DB::Testresult");
Litmus::DB::User->has_many(sessions => "Litmus::DB::Session");

# ZLL: leave this commented until we're ready for Bugzilla login
#Litmus::DB::User->has_a(bugzilla_uid => "Litmus::DB::BugzillaUser");

# returns the crypt'd password from a linked Bugzilla account if it 
# exists or the Litmus user account
sub getRealPasswd {
	my $self = shift;
	if ($self->bugzilla_uid()) {
		return $self->bugzilla_uid()->cryptpassword();
	} else {
		return $self->password();
	}
}

1;
