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
# The Original Code is the Bugzilla Bug Tracking System.
#
# Contributor(s): Marc Schumann <wurblzap@gmail.com>

package Bugzilla::WebService;

use strict;
use Bugzilla::WebService::Constants;

sub fail_unimplemented {
    my $this = shift;

    die SOAP::Fault
        ->faultcode(ERROR_UNIMPLEMENTED)
        ->faultstring('Service Unimplemented');
}

package Bugzilla::WebService::XMLRPC::Transport::HTTP::CGI;

use strict;
eval 'use base qw(XMLRPC::Transport::HTTP::CGI)';

sub make_response {
    my $self = shift;

    $self->SUPER::make_response(@_);

    # XMLRPC::Transport::HTTP::CGI doesn't know about Bugzilla carrying around
    # its cookies in Bugzilla::CGI, so we need to copy them over.
    foreach (@{Bugzilla->cgi->{'Bugzilla_cookie_list'}}) {
        $self->response->headers->push_header('Set-Cookie', $_);
    }
}

1;

__END__

=head1 NAME

Bugzilla::WebSerice - The Web Service interface to Bugzilla

=head1 DESCRIPTION

This is the standard API for external programs that want to interact
with Bugzilla. It provides various methods in various modules.

=head1 STABLE, EXPERIMENTAL, and UNSTABLE

Methods are marked B<STABLE> if you can expect their parameters and
return values not to change between versions of Bugzilla. You are 
best off always using methods marked B<STABLE>. We may add parameters
and additional items to the return values, but your old code will
always continue to work with any new changes we make. If we ever break
a B<STABLE> interface, we'll post a big notice in the Release Notes,
and it will only happen during a major new release.

Methods (or parts of methods) are marked B<EXPERIMENTAL> if 
we I<believe> they will be stable, but there's a slight chance that 
small parts will change in the future.

Certain parts of a method's description may be marked as B<UNSTABLE>,
in which case those parts are not guaranteed to stay the same between
Bugzilla versions.

