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
#                 Dallas Harken <dharken@novell.com>

package Bugzilla::WebService;

use strict;
use Bugzilla::Config;
use Bugzilla::WebService::Constants;

sub fail_unimplemented 
{
    my $this = shift;

    die SOAP::Fault
        ->faultcode(ERROR_UNIMPLEMENTED)
        ->faultstring('Service Unimplemented');
}

sub login
{
    my $self = shift;
    
    # Check for use of iChain first
    if (Param('user_verify_class') ne 'iChain')
    {
        #
        # Check for use of Basic Authorization
        #
        # WARNING - Your must modify your Apache server's configuration
        # to allow the HTTP_AUTHORIZATION env parameter to be passed through!
        # This requires using the rewrite module.
        #
        if (defined($ENV{'HTTP_AUTHORIZATION'})) 
        {
            if ($ENV{'HTTP_AUTHORIZATION'} =~ /^Basic +(.*)$/os) 
            {
                # HTTP Basic Authentication
                my($login, $password) = split(/:/, MIME::Base64::decode_base64($1), 2);
            
                my $cgi = Bugzilla->cgi;
                $cgi->param("Bugzilla_login", $login);
                $cgi->param("Bugzilla_password", $password);
            }
        }
    }
    
    Bugzilla->login; 
}

sub logout
{
    my $self = shift;
    
    Bugzilla->logout;
}

package Bugzilla::WebService::XMLRPC::Transport::HTTP::CGI;

use strict;
use Bugzilla::WebService::Constants;
eval 'use base qw(XMLRPC::Transport::HTTP::CGI)';

sub make_fault 
{
    my $self = shift;

	# RPC Fault Code must be an integer
    $self->SUPER::make_fault(ERROR_FAULT_SERVER, $_[1]);
}

sub make_response 
{
    my $self = shift;

    $self->SUPER::make_response(@_);

    # XMLRPC::Transport::HTTP::CGI doesn't know about Bugzilla carrying around
    # its cookies in Bugzilla::CGI, so we need to copy them over.
    foreach (@{Bugzilla->cgi->{'Bugzilla_cookie_list'}}) {
        $self->response->headers->push_header('Set-Cookie', $_);
    }
}

1;