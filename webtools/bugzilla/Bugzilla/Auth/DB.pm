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
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 Dan Mosedale <dmose@mozilla.org>
#                 Joe Robins <jmrobins@tgix.com>
#                 Dave Miller <justdave@syndicomm.com>
#                 Christopher Aillon <christopher@aillon.com>
#                 Gervase Markham <gerv@gerv.net>
#                 Christian Reis <kiko@async.com.br>
#                 Bradley Baetz <bbaetz@acm.org>

package Bugzilla::Auth::DB;

use strict;

use Bugzilla::Config;
use Bugzilla::Constants;
use Bugzilla::Util;

sub authenticate {
    my ($class, $username, $passwd) = @_;

    return (AUTH_NODATA) unless defined $username && defined $passwd;

    my $dbh = Bugzilla->dbh;

    # We're just testing against the db, so any value is ok
    trick_taint($username);

    # Retrieve the user's ID and crypted password from the database.
    my $sth = $dbh->prepare_cached("SELECT userid,cryptpassword,disabledtext " .
                                   "FROM profiles " .
                                   "WHERE login_name=?");
    my ($userid, $realcryptpwd, $disabledtext) =
      $dbh->selectrow_array($sth,
                            undef,
                            $username);

    # If the user doesn't exist, return now
    return (AUTH_LOGINFAILED) unless defined $userid;

    # OK, now authenticate the user

    # Get the salt from the user's crypted password.
    my $salt = $realcryptpwd;

    # Using the salt, crypt the password the user entered.
    my $enteredCryptedPassword = crypt($passwd, $salt);

    # Make sure the passwords match or return an error
    return (AUTH_LOGINFAILED, $userid) unless
      ($enteredCryptedPassword eq $realcryptpwd);

    # Now we know that the user has logged in successfully,
    # so delete any password tokens for them
    require Token;
    Token::DeletePasswordTokens($userid, "user_logged_in");

    # The user may have had their account disabled
    return (AUTH_DISABLED, $userid, $disabledtext)
      if $disabledtext ne '';

    # If we get to here, then the user is allowed to login, so we're done!
    return (AUTH_OK, $userid);
}

sub can_edit { return 1; }

1;

__END__

=head1 NAME

Bugzilla::Auth::DB - database authentication for Bugzilla

=head1 SUMMARY

This is an L<authentication module|Bugzilla::Auth/"AUTHENTICATION"> for
Bugzilla, which logs the user in using the password stored in the C<profiles>
table. This is the most commonly used authentication module.

=head1 SEE ALSO

L<Bugzilla::Auth>
