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

    # We're just testing against the db: any value is ok
    trick_taint($username);

    my $userid = $class->get_id_from_username($username);
    return (AUTH_LOGINFAILED) unless defined $userid;

    return (AUTH_LOGINFAILED, $userid) 
        unless $class->check_password($userid, $passwd);

    # The user's credentials are okay, so delete any outstanding
    # password tokens they may have generated.
    require Bugzilla::Token;
    Bugzilla::Token::DeletePasswordTokens($userid, "user_logged_in");

    # Account may have been disabled
    my $disabledtext = $class->get_disabled($userid);
    return (AUTH_DISABLED, $userid, $disabledtext)
      if $disabledtext ne '';

    return (AUTH_OK, $userid);
}

sub can_edit { return 1; }

sub get_id_from_username {
    my ($class, $username) = @_;
    my $dbh = Bugzilla->dbh;
    my $sth = $dbh->prepare_cached("SELECT userid FROM profiles " .
                                   "WHERE login_name=?");
    my ($userid) = $dbh->selectrow_array($sth, undef, $username);
    return $userid;
}

sub get_disabled {
    my ($class, $userid) = @_;
    my $dbh = Bugzilla->dbh;
    my $sth = $dbh->prepare_cached("SELECT disabledtext FROM profiles " .
                                   "WHERE userid=?");
    my ($text) = $dbh->selectrow_array($sth, undef, $userid);
    return $text;
}

sub check_password {
    my ($class, $userid, $passwd) = @_;
    my $dbh = Bugzilla->dbh;
    my $sth = $dbh->prepare_cached("SELECT cryptpassword FROM profiles " .
                                   "WHERE userid=?");
    my ($realcryptpwd) = $dbh->selectrow_array($sth, undef, $userid);

    # Get the salt from the user's crypted password.
    my $salt = $realcryptpwd;

    # Using the salt, crypt the password the user entered.
    my $enteredCryptedPassword = crypt($passwd, $salt);

    return $enteredCryptedPassword eq $realcryptpwd;
}

sub change_password {
    my ($class, $userid, $password) = @_;
    my $dbh = Bugzilla->dbh;
    my $cryptpassword = Crypt($password);
    $dbh->do("UPDATE profiles SET cryptpassword = ? WHERE userid = ?", 
             undef, $cryptpassword, $userid);
}

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
