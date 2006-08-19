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
