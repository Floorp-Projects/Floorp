# -*- Mode: perl; tab-width: 4; indent-tabs-mode: nil; -*-
#
# This file is MPL/GPL dual-licensed under the following terms:
# 
# The contents of this file are subject to the Mozilla Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
# the License for the specific language governing rights and
# limitations under the License.
#
# The Original Code is PLIF 1.0.
# The Initial Developer of the Original Code is Ian Hickson.
#
# Alternatively, the contents of this file may be used under the terms
# of the GNU General Public License Version 2 or later (the "GPL"), in
# which case the provisions of the GPL are applicable instead of those
# above. If you wish to allow use of your version of this file only
# under the terms of the GPL and not to allow others to use your
# version of this file under the MPL, indicate your decision by
# deleting the provisions above and replace them with the notice and
# other provisions required by the GPL. If you do not delete the
# provisions above, a recipient may use your version of this file
# under either the MPL or the GPL.

package PLIF::ProtocolHelper::Logout::HTTP;
use strict;
use vars qw(@ISA);
use PLIF::Service;
@ISA = qw(PLIF::Service);
1;

sub provides {
    my $class = shift;
    my($service) = @_;
    return ($service eq 'user.login.canLogin.http' or
            $service eq 'user.login.required.http' or
            $service eq 'user.logout.http' or
            $service eq 'user.fieldRegisterer' or
            $class->SUPER::provides($service));
}

__DATA__

sub objectProvides {
    my $self = shift;
    my($service) = @_;
    return ($service eq 'user.login.loggedOutUserHandle.http' or
            $self->SUPER::provides($service));
}

sub objectInit {
    my $self = shift;
    my($app, $user) = @_;
    $self->SUPER::objectInit(@_);
    $self->user($user);
}

# user.login.canLogin.<protocol>
sub canLogin {
    my $self = shift;
    my($app, $user) = @_;
    my $state = $user->hasField('state', 'http.logout');
    if (defined($state)) {
        $app->addObject($self->objectCreate($app, $user));
        return 0;
    }
    return;
}

# user.login.required.<protocol>
sub loginRequired {
    my $self = shift;
    my($app) = @_;
    my $userHandle = $app->getObject('user.login.loggedOutUserHandle.http');
    if (defined($userHandle)) {
        my $state = $userHandle->user->hasField('state', 'http.logout');
        if (defined($state)) {
            my $value = $state->data - 1;
            if ($value > 0) {
                $state->data($value);
            } else {
                $state->remove();
            }
        }
    }
    return;
}

# user.logout.<protocol>
sub logoutUser {
    my $self = shift;
    my($app, $user) = @_;
    $user->getField('state', 'http.logout')->data(2);
}

# user.fieldRegisterer
sub register {
    my $self = shift;
    my($app, $fieldFactory) = @_;
    $fieldFactory->registerField($app, 'state', 'http.logout', 'integer');
}
