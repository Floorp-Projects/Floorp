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

package PLIF::Service::Components::Login;
use strict;
use vars qw(@ISA);
use PLIF::Service;
@ISA = qw(PLIF::Service);
1;

sub provides {
    my $class = shift;
    my($service) = @_;
    return ($service eq 'input.verify' or 
            $service eq 'input.verify.user.generic' or 
            $service eq 'user.login' or
            $service eq 'component.userLogin' or
            $service eq 'dispatcher.commands' or 
            $service eq 'dispatcher.output.generic' or 
            $service eq 'dispatcher.output' or 
            $service eq 'dataSource.strings.default' or
            $class->SUPER::provides($service));
}

# input.verify
sub verifyInput {
    my $self = shift;
    my($app) = @_;
    # let's see if there are any protocol-specific user authenticators
    my @result = $app->getSelectingServiceList('input.verify.user.'.$app->input->defaultOutputProtocol)->authenticateUser($app);
    if (not @result) { 
        # ok, let's try the generic authenticators...
        @result = $app->getSelectingServiceList('input.verify.user.generic')->authenticateUser($app);
    }
    # now let's see what that gave us
    if (@result) {
        # horrah, somebody knew what to do!
        if (defined($result[0])) {
            $app->addObject($result[0]); # they will have returned a user object
        } else {
            # hmm, so apparently user is not authentic
            return $self; # supports user.login (reportInputVerificationError)
        }
    }
    return; # nope, nothing to see here... (no error, anyway)
}

# input.verify.user.generic
sub authenticateUser {
    my $self = shift;
    my($app) = @_;
    if (defined($app->input->username)) {
        return $app->getService('user.factory')->getUserByCredentials($app, $app->input->username, $app->input->password);
    } else {
        return; # return nothing (not undef)
    }
}

# input.verify
sub reportInputVerificationError {
    my $self = shift;
    my($app) = @_;
    $app->output->loginFailed(1); # 1 means 'unknown username/password'
}

# cmdSendPassword could also be called 'cmdNewUser'
# dispatcher.commands
sub cmdSendPassword {
    my $self = shift;
    my($app) = @_;
    my $protocol = $app->input->getArgument('protocol');
    my $address = $app->input->getArgument('address');
    if (defined($protocol) and defined($address)) {
        my $user = $app->getService('user.factory')->getUserByContactDetails($app, $protocol, $address);
        my $password;
        if (defined($user)) {
            $password = $self->changePassword($app, $user);
        } else {
            ($user, $password) = $self->createUser($app, $protocol, $address);
            if (not defined($user)) {
                $app->output->loginFailed(2); # 2 means 'invalid protocol/username'
                return;
            }
        }
        $self->sendPassword($app, $user, $protocol, $password);
    } else {
        $app->output->loginFailed(0); # 0 means 'no username/password'
    }
}

# user.login
# if this returns undef, don't do anything!
# XXX need a quieter version of this to enable/disable UI elements
sub hasRight {
    my $self = shift;
    my($app, $right) = @_;
    my $user = $app->getObject('user');
    if (defined($user)) {
        if ($user->hasRight($right)) {
            return $user;
        } else {
            $app->output->loginInsufficient($right);
        }
    } else {
        $self->requireLogin($app);
    }
    return undef;
}

# user.login
# this assumes user is not logged in
sub requireLogin {
    my $self = shift;
    my($app) = @_;
    my $address = $app->input->address;
    if (defined($address) and not defined($app->getService('user.factory')->getUserByContactDetails($app, $app->input->protocol, $address))) {
        my($user, $password) = $self->createUser($app, $app->input->protocol, $address);
        $self->sendPassword($app, $user, $app->input->protocol, $password);
    } else {
        $app->output->loginFailed(0);
    }
}

# dispatcher.output.generic
sub outputLoginInsufficient {
    my $self = shift;
    my($app, $output, $right) = @_;
    $output->output('login.accessDenied', {
        'right' => $right,
    });   
}

# dispatcher.output.generic
sub outputLoginFailed {
    my $self = shift;
    my($app, $output, $tried) = @_;
    $output->output('login.failed', {
        'tried' => $tried, # 0 = no username; 1 = unknown username; 2 = invalid username
        'contacts' => 0, # XXX should fill this in
    });   
}

# dispatcher.output.generic
sub outputLoginDetailsSent {
    my $self = shift;
    my($app, $output, $address, $protocol) = @_;
    $output->output('login.detailsSent', {
        'address' => $address,
        'protocol' => $protocol,
    });   
}

# dispatcher.output.generic
sub outputLoginDetails {
    my $self = shift;
    my($app, $output, $username, $password) = @_;
    $output->output('login.details', {
        'username' => $username,
        'password' => $password,
    });   
}

# dispatcher.output
sub strings {
    return (
            'login.accessDenied' => 'Displayed when the user does not have the requisite right (namely, data.right)',
            'login.failed' => 'Displayed when the user has not logged in (data.tried is false) or when the credentials were wrong (data.tried is true)',
            'login.detailsSent' => 'The password was sent to data.address using data.protocol',
            'login.details' => 'The message containing the data.username and data.password of a new account or when the user has forgotten his password (only required for contact protocols, e.g. e-mail)',
            );
}

# dataSource.strings.default
sub getDefaultString {
    my $self = shift;
    my($app, $protocol, $string) = @_;
    if ($protocol eq 'stdout') {
        if ($string eq 'login.accessDenied') {
            return ('COSES', '<text xmlns="http://bugzilla.mozilla.org/coses">Access Denied<br/></text>');
        } elsif ($string eq 'login.failed') {
            return ('COSES', '<text xmlns="http://bugzilla.mozilla.org/coses"><if lvalue="(data.tried)" condition="!=" rvalue="0">Wrong username or password.</if><else>You must give your username or password.</else><br/><!-- XXX offer to create an account or send the password --><br/></text>');
        } elsif ($string eq 'login.detailsSent') {
            return ('COSES', '<text xmlns="http://bugzilla.mozilla.org/coses">Login details were sent. (Protocol: <text value="(data.protocol)"/>; Address: <text value="(data.address)"/>)<br/></text>');
        }
    } elsif ($protocol eq 'http') {
        if ($string eq 'login.accessDenied') {
            return ('COSES', '<text xmlns="http://bugzilla.mozilla.org/coses">HTTP/1.1 401 Access Denied<br/>Content-Type: text/plain<br/><br/>Access Denied</text>');
        } elsif ($string eq 'login.failed') {
            return ('COSES', '<text xmlns="http://bugzilla.mozilla.org/coses">HTTP/1.1 401 Login Required<br/>WWW-Authenticate: Basic realm="<text value="(data.app.name)"/>"<br/>Content-Type: text/plain<br/><br/><if lvalue="(data.tried)" condition="!=" rvalue="0">Wrong username or password.</if><else>You must give your username or password.</else><br/><!-- XXX offer to create an account or send the password --></text>');
        } elsif ($string eq 'login.detailsSent') {
            return ('COSES', '<text xmlns="http://bugzilla.mozilla.org/coses">HTTP/1.1 200 OK<br/>Content-Type: text/plain<br/><br/>Login details were sent.<br/>Protocol: <text value="(data.protocol)"/><br/>Address: <text value="(data.address)"/></text>');
        }
    }
    return; # nope, sorry
}


# internal routines

sub changePassword {
    my $self = shift;
    my($app, $user) = @_;
    my($crypt, $password) = $app->getService('service.passwords')->newPassword();
    $user->password($crypt);
    return $password;
}

sub createUser {
    my $self = shift;
    my($app, $protocol, $address) = @_;
    my $validator = $app->getService('protocol.'.$protocol);
    if ((defined($validator)) and (not $validator->checkAddress($address))) {
        return (undef, undef);
    }
    my($crypt, $password) = $app->getService('service.passwords')->newPassword();
    my $user = $app->getService('user.factory')->getNewUser($app, $crypt);
    $user->getField('contact', $protocol)->data($address);
    return ($user, $password);
}

sub sendPassword {
    my $self = shift;
    my($app, $user, $protocol, $password) = @_;
    my $field = $user->hasField('contact', $protocol);
    $self->assert(defined($field), 1, 'Tried to send a password using a protocol that the user has no mention of!'); # XXX grammar... :-)
    $app->output($protocol, $user)->loginDetails($field->username, $password);
    $app->output->loginDetailsSent($field->address, $protocol);
}
