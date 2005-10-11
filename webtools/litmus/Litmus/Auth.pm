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

package Litmus::Auth;

use strict;

require Exporter;
use Litmus;
use Litmus::DB::User;
use Litmus::Error;

use CGI;

our @ISA = qw(Exporter);
our @EXPORT = qw();

my $logincookiename = $Litmus::Config::user_cookiename;

sub setCookie {
    my $user = shift;
    
    my $c = new CGI;
    
    my $cookie = $c->cookie( 
        -name   => $logincookiename,
        -value  => $user->userid(),
        -domain => $main::ENV{"HTTP_HOST"},
        -expires=>'+3d',
    );
    
    return $cookie;
}

sub getCookie() {
    my $c = new CGI;
    
    my $cookie = $c->cookie($logincookiename);
    
    my $user = Litmus::DB::User->retrieve($cookie);
    if (! $user) {
        return 0;
    } else {
        unless ($user->userid() == $cookie) {
            invalidInputError("Invalid login cookie");
        }
    }
    return $user;
}

sub istrusted($) {
    my $userobj = shift;

    return 0 if (!$userobj);
    
    if ($userobj->istrusted()) {
        return 1;
    } else {
        return 0;
    }
}

sub canEdit($) {
    my $userobj = shift;
    
    return $userobj->istrusted();
}

1;
