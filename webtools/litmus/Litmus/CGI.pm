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
 #   Zach Lipton <zach@zachlipton.com>
 #
 # ***** END LICENSE BLOCK *****

=cut

package Litmus::CGI; 

use strict;
use base 'CGI';

# Subclass of CGI.pm that can store cookies in advance and output them 
# later when the header() method is called. This feature should probably 
# be submitted as a patch to CGI.pm itself.

sub new {
	my $class = shift;
	my @args = @_;
	
	my $self = $class->SUPER::new(@args);
	$self->{'litmusCookieStore'} = [];
	return $self;
}

# Stores a cookie to be set later by the header() method
sub storeCookie {
	my $self = shift;
	my $cookie = shift;
	
	# "we're like kids in a candy shop"
	my @cookieStore = @{$self->{'litmusCookieStore'}};
	push(@cookieStore, $cookie);
	$self->{'litmusCookieStore'} = \@cookieStore;
}

sub header {
	my $self = shift;
	my @args = @_;
	
	foreach my $cur ($self->{'litmusCookieStore'}) {
		push(@args, {-cookie => $cur});
	}
	
	$self->SUPER::header(@args);
}

1;
