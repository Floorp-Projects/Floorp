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
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Bradley Baetz <bbaetz@acm.org>

package Bugzilla::Auth;

use strict;

use Bugzilla::Config;
use Bugzilla::Constants;

# 'inherit' from the main loginmethod
BEGIN {
    my $loginmethod = Param("loginmethod");
    if ($loginmethod =~ /^([A-Za-z0-9_\.\-]+)$/) {
        $loginmethod = $1;
    }
    else {
        die "Badly-named loginmethod '$loginmethod'";
    }
    require "Bugzilla/Auth/" . $loginmethod . ".pm";

    our @ISA;
    push (@ISA, "Bugzilla::Auth::" . $loginmethod);
}

# PRIVATE

# Returns the network address for a given ip
sub get_netaddr {
    my $ipaddr = shift;

    # Check for a valid IPv4 addr which we know how to parse
    if (!$ipaddr || $ipaddr !~ /^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$/) {
        return undef;
    }

    my $addr = unpack("N", pack("CCCC", split(/\./, $ipaddr)));

    my $maskbits = Param('loginnetmask');

    $addr >>= (32-$maskbits);
    $addr <<= (32-$maskbits);
    return join(".", unpack("CCCC", pack("N", $addr)));
}

1;

__END__

=head1 NAME

Bugzilla::Auth - Authentication handling for Bugzilla users

=head1 DESCRIPTION

Handles authentication for Bugzilla users.

Authentication from Bugzilla involves two sets of modules. One set is
used to obtain the data (from CGI, email, etc), and the other set uses
this data to authenticate against the datasource (the Bugzilla DB, LDAP,
cookies, etc).

The handlers for the various types of authentication
(DB/LDAP/cookies/etc) provide the actual code for each specific method
of authentication.

The source modules (currently, only
L<Bugzilla::Auth::CGI|Bugzilla::Auth::CGI>) then use those methods to do
the authentication.

I<Bugzilla::Auth> itself inherits from the default authentication handler,
identified by the I<loginmethod> param.

=head1 METHODS

C<Bugzilla::Auth> contains several helper methods to be used by
authentication or login modules.

=over 4

=item C<Bugzilla::Auth::get_netaddr($ipaddr)>

Given an ip address, this returns the associated network address, using
C<Param('loginnetmask')> as the netmask. This can be used to obtain data
in order to restrict weak authentication methods (such as cookies) to
only some addresses.

=back

=head1 AUTHENTICATION

Authentication modules check a user's credentials (username, password,
etc) to verify who the user is.

=head2 METHODS

=over 4

=item C<authenticate($username, $pass)>

This method is passed a username and a password, and returns a list
containing up to four return values, depending on the results of the
authentication.

The first return value is one of the status codes defined in
L<Bugzilla::Constants|Bugzilla::Constants> and described below.  The
rest of the return values are status code-specific and are explained in
the status code descriptions.

=over 4

=item C<AUTH_OK>

Authentication succeeded. The second variable is the userid of the new
user.

=item C<AUTH_NODATA>

Insufficient login data was provided by the user. This may happen in several
cases, such as cookie authentication when the cookie is not present.

=item C<AUTH_ERROR>

An error occurred when trying to use the login mechanism. The second return
value may contain the Bugzilla userid, but will probably be C<undef>,
signifiying that the userid is unknown. The third value is a tag describing
the error used by the authentication error templates to print a description
to the user. The optional fourth argument is a hashref of values used as part
of the tag's error descriptions.

This error template must have a name/location of
I<account/auth/C<lc(authentication-type)>-error.html.tmpl>.

=item C<AUTH_LOGINFAILED>

An incorrect username or password was given. Note that for security reasons,
both cases return the same error code. However, in the case of a valid
username, the second argument may be the userid. The authentication
mechanism may not always be able to discover the userid if the password is
not known, so whether or not this argument is present is implementation
specific. For security reasons, the presence or lack of a userid value should
not be communicated to the user.

The third argument is an optional tag from the authentication server
describing the error. The tag can be used by a template to inform the user
about the error.  Similar to C<AUTH_ERROR>, an optional hashref may be
present as a fourth argument, to be used by the tag to give more detailed 
information.

=item C<AUTH_DISABLED>

The user successfully logged in, but their account has been disabled.
The second argument in the returned array is the userid, and the third
is some text explaining why the account was disabled. This text would
typically come from the C<disabledtext> field in the C<profiles> table.
Note that this argument is a string, not a tag.

=back

=item C<can_edit>

This determines if the user's account details can be modified. If this
method returns a C<true> value, then accounts can be created and
modified through the Bugzilla user interface. Forgotten passwords can
also be retrieved through the L<Token interface|Bugzilla::Token>.

=back

=head1 LOGINS

A login module can be used to try to log in a Bugzilla user in a
particular way. For example, L<Bugzilla::Auth::CGI|Bugzilla::Auth::CGI>
logs in users from CGI scripts, first by using form variables, and then
by trying cookies as a fallback.

The login interface consists of the following methods:

=item C<login>, which takes a C<$type> argument, using constants found in
C<Bugzilla::Constants>.

The login method may use various authentication modules (described
above) to try to authenticate a user, and should return the userid on
success, or C<undef> on failure.

When a login is required, but data is not present, it is the job of the
login method to prompt the user for this data.

The constants accepted by C<login> include the following:

=over 4

=item C<LOGIN_OPTIONAL>

A login is never required to access this data. Attempting to login is
still useful, because this allows the page to be personalised. Note that
an incorrect login will still trigger an error, even though the lack of
a login will be OK.

=item C<LOGIN_NORMAL>

A login may or may not be required, depending on the setting of the
I<requirelogin> parameter.

=item C<LOGIN_REQUIRED>

A login is always required to access this data.

=back

=item C<logout>, which takes a C<Bugzilla::User> argument for the user
being logged out, and an C<$option> argument. Possible values for
C<$option> include:

=over 4

=item C<LOGOUT_CURRENT>

Log out the user and invalidate his currently registered session.

=item C<LOGOUT_ALL>

Log out the user, and invalidate all sessions the user has registered in
Bugzilla.

=item C<LOGOUT_KEEP_CURRENT>

Invalidate all sessions the user has registered excluding his current
session; this option should leave the user logged in. This is useful for
user-performed password changes.

=back

=head1 SEE ALSO

L<Bugzilla::Auth::CGI>, L<Bugzilla::Auth::Cookie>, L<Bugzilla::Auth::DB>

