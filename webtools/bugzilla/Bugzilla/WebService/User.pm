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
#                 Max Kanat-Alexander <mkanat@bugzilla.org>

package Bugzilla::WebService::User;

use strict;
use base qw(Bugzilla::WebService);

import SOAP::Data qw(type); 

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::User;
use Bugzilla::Util qw(trim);
use Bugzilla::Token;

##############
# User Login #
##############

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

#################
# User Creation #
#################

sub offer_account_by_email {
    my $self = shift;
    my ($params) = @_;
    my $email = trim($params->{email})
        || ThrowCodeError('param_required', { param => 'email' });

    $email = Bugzilla::User->check_login_name_for_creation($email);

    # Create and send a token for this new account.
    Bugzilla::Token::issue_new_user_account_token($email);

    return undef;
}

sub create {
    my $self = shift;
    my ($params) = @_;

    Bugzilla->user->in_group('editusers') 
        || ThrowUserError("auth_failure", { group  => "editusers",
                                            action => "add",
                                            object => "users"});

    my $email = trim($params->{email})
        || ThrowCodeError('param_required', { param => 'email' });
    my $realname = trim($params->{full_name});
    my $password = trim($params->{password}) || '*';

    my $user = Bugzilla::User->create({
        login_name    => $email,
        realname      => $realname,
        cryptpassword => $password
    });

    return { user_id => type('int')->value($user->id) };
}

1;

__END__

=head1 NAME

Bugzilla::Webservice::User - The User Account and Login API

=head1 DESCRIPTION

This part of the Bugzilla API allows you to create User Accounts.

=head1 METHODS

See L<Bugzilla::WebService> for a description of what B<STABLE>, B<UNSTABLE>,
and B<EXPERIMENTAL> mean, and for more information about error codes.

=head2 Account Creation

=over

=item C<offer_account_by_email> B<EXPERIMENTAL>

Description: Sends an email to the user, offering to create an account.
             The user will have to click on a URL in the email, and
             choose their password and real name.
             This is the recommended way to create a Bugzilla account.

Params:     C<email> - The email to send the offer to.

Returns:    nothing

=over

=item 500 (Illegal Email Address)

This Bugzilla does not allow you to create accounts with the format of
email address you specified. Account creation may be entirely disabled.

=item 501 (Account Already Exists)

An account with that email address already exists in Bugzilla.

=back

=item C<create> B<EXPERIMENTAL>

Description: Creates a user account directly in Bugzilla, password and all.
             Instead of this, you should use L</offer_account_by_email>
             when possible, because that makes sure that the email address
             specified can actually receive an email. This function
             does not check that.

Params:      C<email> B<Required> - The email address for the new user.
             C<full_name> - A string, the user's full name. Will be
                 set to empty if not specified.
             C<password> - The password for the new user account, in
                 plain text. It will be stripped of leading and trailing
                 whitespace. If blank or not specified, the newly
                 created account will exist in Bugzilla, but will not
                 be allowed to log in using DB authentication until a
                 password is set either by the user (through resetting
                 their password) or by the administrator.

Returns:    A hash containing one item, C<user_id>, the numeric id of
            the user that was created.

Errors:     The same as L</offer_account_by_email>. If a password
            is specified, the function may also throw:

=over

=item 502 (Password Too Short)

The password specified is too short. (Usually, this means the
password is under three characters.)

=item 503 (Password Too Long)

The password specified is too long. (Usually, this means the
password is over ten characters.)

=back

=back
