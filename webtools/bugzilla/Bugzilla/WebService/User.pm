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

package Bugzilla::WebService::User;

use strict;
use base qw(Bugzilla::WebService);
use Bugzilla;
use Bugzilla::Constants;

sub login {
    my $self = shift;
    my ($login, $password, $remember) = @_;

    # Convert $remember from a boolean 0/1 value to a CGI-compatible one.
    if (defined($remember)) {
        $remember = $remember? 'on': '';
    }
    else {
        # Use Bugzilla's default if $remember is not supplied.
        $remember =
            Bugzilla->params->{'rememberlogin'} eq 'defaulton'? 'on': '';
    }

    # Make sure the CGI user info class works if necessary.
    my $cgi = Bugzilla->cgi;
    $cgi->param('Bugzilla_login', $login);
    $cgi->param('Bugzilla_password', $password);
    $cgi->param('Bugzilla_remember', $remember);

    Bugzilla->login;
    return Bugzilla->user->id;
}

sub logout {
    my $self = shift;

    Bugzilla->login(LOGIN_OPTIONAL);
    Bugzilla->logout;
}

1;
