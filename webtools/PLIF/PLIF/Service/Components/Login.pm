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
            $service eq 'setup.install' or
            $class->SUPER::provides($service));
}

# this module makes calls to:
#  input.verify.user.<protocol>
#  user.login.canLogin.<protocol>
#  user.logout.<protocol>
#  user.login.required.<protocol>

# input.verify
sub verifyInput {
    my $self = shift;
    my($app) = @_;
    # clear internal flags
    $self->userAdminMessage('');
    # let's see if there are any protocol-specific user authenticators
    my @result = $app->getSelectingServiceList('input.verify.user.'.$app->input->defaultOutputProtocol)->authenticateUser($app);
    if (not @result) {
        # ok, let's try the generic authenticators...
        # this should not fail since it will fall back on this module
        @result = $app->getSelectingServiceList('input.verify.user.generic')->authenticateUser($app);
    }
    # now let's see what that gave us
    if (@result) {
        # horrah, somebody knew what to do!
        if (defined($result[0])) {
            if ($result[0]->checkLogin()) {
                # hook for things that want to override login
                if (not($app->getSelectingServiceList('user.login.canLogin.'.$app->input->defaultOutputProtocol)->canLogin($app, $result[0]) or
                        $app->getSelectingServiceList('user.login.canLogin.generic')->canLogin($app, $result[0]))) {
                    # can definitely log in
                    $app->addObject($result[0]); # they will have returned a user object
                }
                # else user is not denied but protocol-specific stuff
                # made it not count
            } else {
                # hmm, so apparently user is not allowed to log in
                $self->dump(2, 'user '.($result[0]->userID).' tried logging in but their account is disabled');
                $self->userAdminMessage($result[0]->adminMessage);
                return $self; # supports user.login (reportInputVerificationError)
            }
        }
        # else user did not try to authenticate
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
    $app->output->loginFailed(1, $self->userAdminMessage); # 1 means 'unknown username/password'
}

# dispatcher.commands
sub cmdLogin {
    my $self = shift;
    my($app) = @_;
    my $user = $app->getObject('user');
    if (defined($user)) {
        $app->noCommand();
    } else {
        $self->requireLogin($app);
    }
}

# dispatcher.commands
sub cmdLoginRequestAccount {
    my $self = shift;
    my($app) = @_;
    $app->output->loginRequestAccount();
}

# dispatcher.commands
sub cmdLoginLogout {
    my $self = shift;
    my($app) = @_;
    my $user = $app->getObject('user');
    if (defined($user)) {
        $self->assert($app->getSelectingServiceList('user.logout.'.$app->input->defaultOutputProtocol)->logoutUser($app, $user) or
                      $app->getSelectingServiceList('user.logout.generic')->logoutUser($app, $user),
                      0, 'Logging out when using the '.($app->input->defaultOutputProtocol).' protocol is not supported');
        $app->removeObject($user);
    } else {
        $self->warn(4, 'tried to log out but was not logged in');
    }
    $app->noCommand();
}

# cmdLoginSendPassword could also be called 'cmdLoginNewUser'
# dispatcher.commands
sub cmdLoginSendPassword {
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
                $app->output->loginFailed(2, ''); # 2 means 'invalid protocol/username'
                return;
            }
        }
        $self->sendPassword($app, $user, $protocol, $password);
    } else {
        $app->output->loginFailed(0, ''); # 0 means 'no username/password'
    }
}

# user.login
# if this returns undef, don't do anything!
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
    # hook for things that want to know that we've denied access
    if (not $app->getSelectingServiceList('user.login.required.'.$app->input->defaultOutputProtocol)->loginRequired($app)) {
        $app->getSelectingServiceList('user.login.required.generic')->loginRequired($app);
    }
    my $address = $app->input->address;
    if (defined($address) and
        not defined($app->getService('user.factory')->getUserByContactDetails($app, $app->input->protocol, $address))) {
        my($user, $password) = $self->createUser($app, $app->input->protocol, $address);
        $self->sendPassword($app, $user, $app->input->protocol, $password);
        return;
    }
    $app->output->loginFailed(0, '');
}

# dispatcher.output.generic
sub outputLoginInsufficient {
    my $self = shift;
    my($app, $output, $right) = @_;
    $output->output('login.accessDenied', {
        'right' => $right,
    });   
}

sub getPendingCommandString {
    my $self = shift;
    my($app) = @_;
    my $pendingCommand = $app->input->peekArgument('loginPendingCommands');
    if (not defined($pendingCommand)) {
        $pendingCommand = $app->input->getArgumentsAsString();
    }
    return $pendingCommand;
}

# dispatcher.output.generic
sub outputLoginFailed {
    my $self = shift;
    my($app, $output, $tried, $message) = @_;
    my $pendingCommand = $self->getPendingCommandString($app);
    $output->output('login.failed', {
        'tried' => $tried, # 0 = no username; 1 = unknown username; 2 = invalid username
        'contacts' => [$app->getService('dataSource.user')->getFieldNamesByCategory($app, 'contact')],
        'message' => $message,
        'pendingCommands' => $pendingCommand,
    });
}

# dispatcher.output.generic
sub outputLoginRequestAccount {
    my $self = shift;
    my($app, $output, $tried) = @_;
    my $pendingCommand = $self->getPendingCommandString($app);
    $output->output('login.requestAccount', {
        'contacts' => [$app->getService('dataSource.user')->getFieldNamesByCategory($app, 'contact')],
        'pendingCommands' => $pendingCommand,
    });
}

# dispatcher.output.generic
sub outputLoginDetailsSent {
    my $self = shift;
    my($app, $output, $address, $protocol) = @_;
    $output->output('login.detailsSent', {
        'address' => $address,
        'protocol' => $protocol,
        'pendingCommands' => $app->input->getArgumentsFromString($app->input->peekArgument('loginPendingCommands')),
    });
}

# dispatcher.output.generic
sub outputLoginDetails {
    my $self = shift;
    my($app, $output, $username, $password) = @_;
    $output->output('login.details', {
        'username' => $username,
        'password' => $password,
        'pendingCommands' => $app->input->getArgumentsFromString($app->input->peekArgument('loginPendingCommands')),
    });
}

# dispatcher.output
sub strings {
    return (
            'login.accessDenied' => 'Displayed when the user does not have the requisite right (namely, right).',
            'login.failed' => 'Displayed when the user has not logged in (tried is false) or when the credentials were wrong (tried is true). A message may be given in message.',
            'login.requestAccount' => 'Displayed when the user requests the form to enter a new account (should display the same form as login.failed, basically). Pass pendingCommands back as loginPendingCommands.',
            'login.detailsSent' => 'The password was sent to address using protocol.',
            'login.details' => 'The message containing the username and password of a new account or when the user has forgotten his password (only required for contact protocols, e.g. e-mail).',
            );
}

sub setupInstall {
    my $self = shift;
    my($app) = @_;

    # cache the data source service
    my $dataSource = $app->getService('dataSource.user');

    # look for users in group 1
    my $users = $dataSource->getGroupMembers($app, 1);
    if (not @$users) {
        # no administrators, so ask for one
        $app->output->setupProgress('user.login.administrator');

        # get the contact method
        # XXX should be a way to make sure default contact method is a particular contact method (namely, e-mail)
        my $contact = $app->input->getArgument('user.login.administrator.contactMethod', $dataSource->getFieldNamesByCategory($app, 'contact'));
        if (not defined($contact)) {
            return 'user.login.administrator.contact';
        }

        # get the address of the administrator user
        my $address = $app->input->getArgument('user.login.administrator.address');
        if (not defined($address)) {
            return 'user.login.administrator.address';
        }

        # cache the user factory
        my $userFactory = $app->getService('user.factory');

        # get the user in question
        my $user = $userFactory->getUserByContactDetails($app, $contact, $address);

        # if doesn't exist, create it
        if (not defined($user)) {
            $app->output->setupProgress('user.login.administrator.creating');
            my $password;
            ($user, $password) = $self->createUser($app, $contact, $address);
            if (not defined($user)) {
                return 'user.login.administrator.address';
            }
            $app->output($contact, $user)->loginDetails($address, $password);
        }

        # add the user to group 1 as an owner (so he's the owner of administrators)
        $user->joinGroup(1, 3); # XXX HARDCODED CONSTANT ALERT

        # add an adminMessage to the user
        $user->adminMessage('Logged in with administrative privileges, please be careful.');
    }

    return;
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
    my $validator = $app->getService("protocol.$protocol");
    if ((defined($validator)) and (not $validator->checkAddress($app, $address))) {
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
