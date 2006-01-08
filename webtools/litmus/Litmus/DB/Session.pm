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

package Litmus::DB::Session;

use strict;
use base 'Litmus::DBI';

use Time::Piece;

Litmus::DB::Session->table('sessions');

Litmus::DB::Session->columns(All => qw/session_id user_id sessioncookie expires/);

Litmus::DB::Session->has_a(user_id => "Litmus::DB::User");

Litmus::DB::Session->autoinflate(dates => 'Time::Piece');

# expire the current Session object 
sub makeExpire {
	my $self = shift;
	$self->delete();
}

sub isValid {
	my $self = shift;
	
	if ($self->expires() <= localtime()) {
    	$self->makeExpire();
    	return 0;
    }
    
    if ($self->user_id()->disabled() && $self->user_id()->disabled() == 1) {
    	$self->makeExpire();
    	return 0;
    }
    return 1;
}

1;
