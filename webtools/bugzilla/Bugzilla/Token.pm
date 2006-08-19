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
# Contributor(s):    Myk Melez <myk@mozilla.org>

################################################################################
# Module Initialization
################################################################################

# Make it harder for us to do dangerous things in Perl.
use strict;

# Bundle the functions in this file together into the "Bugzilla::Token" package.
package Bugzilla::Token;

use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Mailer;
use Bugzilla::Util;

use Date::Format;
use Date::Parse;

################################################################################
# Public Functions
################################################################################

# Creates and sends a token to create a new user account.
# It assumes that the login has the correct format and is not already in use.
sub issue_new_user_account_token {
    my $login_name = shift;
    my $dbh = Bugzilla->dbh;
    my $template = Bugzilla->template;
    my $vars = {};

    # Is there already a pending request for this login name? If yes, do not throw
    # an error because the user may have lost his email with the token inside.
    # But to prevent using this way to mailbomb an email address, make sure
    # the last request is at least 10 minutes old before sending a new email.
    trick_taint($login_name);

    my $pending_requests =
        $dbh->selectrow_array('SELECT COUNT(*)
                                 FROM tokens
                                WHERE tokentype = ?
                                  AND ' . $dbh->sql_istrcmp('eventdata', '?') . '
                                  AND issuedate > NOW() - ' . $dbh->sql_interval(10, 'MINUTE'),
                               undef, ('account', $login_name));

    ThrowUserError('too_soon_for_new_token', {'type' => 'account'}) if $pending_requests;

    my ($token, $token_ts) = _create_token(undef, 'account', $login_name);

    $vars->{'email'} = $login_name . Bugzilla->params->{'emailsuffix'};
    $vars->{'token_ts'} = $token_ts;
    $vars->{'token'} = $token;

    my $message;
    $template->process('account/email/request-new.txt.tmpl', $vars, \$message)
      || ThrowTemplateError($template->error());

    MessageToMTA($message);
}

sub IssueEmailChangeToken {
    my ($userid, $old_email, $new_email) = @_;
    my $email_suffix = Bugzilla->params->{'emailsuffix'};

    my ($token, $token_ts) = _create_token($userid, 'emailold', $old_email . ":" . $new_email);

    my $newtoken = _create_token($userid, 'emailnew', $old_email . ":" . $new_email);

    # Mail the user the token along with instructions for using it.

    my $template = Bugzilla->template;
    my $vars = {};

    $vars->{'oldemailaddress'} = $old_email . $email_suffix;
    $vars->{'newemailaddress'} = $new_email . $email_suffix;
    
    $vars->{'max_token_age'} = MAX_TOKEN_AGE;
    $vars->{'token_ts'} = $token_ts;

    $vars->{'token'} = $token;
    $vars->{'emailaddress'} = $old_email . $email_suffix;

    my $message;
    $template->process("account/email/change-old.txt.tmpl", $vars, \$message)
      || ThrowTemplateError($template->error());

    MessageToMTA($message);

    $vars->{'token'} = $newtoken;
    $vars->{'emailaddress'} = $new_email . $email_suffix;

    $message = "";
    $template->process("account/email/change-new.txt.tmpl", $vars, \$message)
      || ThrowTemplateError($template->error());

    MessageToMTA($message);
}

# Generates a random token, adds it to the tokens table, and sends it
# to the user with instructions for using it to change their password.
sub IssuePasswordToken {
    my $loginname = shift;
    my $dbh = Bugzilla->dbh;
    my $template = Bugzilla->template;
    my $vars = {};

    # Retrieve the user's ID from the database.
    trick_taint($loginname);
    my ($userid, $too_soon) =
        $dbh->selectrow_array('SELECT profiles.userid, tokens.issuedate
                                 FROM profiles
                            LEFT JOIN tokens
                                   ON tokens.userid = profiles.userid
                                  AND tokens.tokentype = ?
                                  AND tokens.issuedate > NOW() - ' .
                                      $dbh->sql_interval(10, 'MINUTE') . '
                                WHERE ' . $dbh->sql_istrcmp('login_name', '?'),
                                undef, ('password', $loginname));

    ThrowUserError('too_soon_for_new_token', {'type' => 'password'}) if $too_soon;

    my ($token, $token_ts) = _create_token($userid, 'password', $::ENV{'REMOTE_ADDR'});

    # Mail the user the token along with instructions for using it.
    $vars->{'token'} = $token;
    $vars->{'emailaddress'} = $loginname . Bugzilla->params->{'emailsuffix'};

    $vars->{'max_token_age'} = MAX_TOKEN_AGE;
    $vars->{'token_ts'} = $token_ts;

    my $message = "";
    $template->process("account/password/forgotten-password.txt.tmpl", 
                                                               $vars, \$message)
      || ThrowTemplateError($template->error());

    MessageToMTA($message);
}

sub IssueSessionToken {
    # Generates a random token, adds it to the tokens table, and returns
    # the token to the caller.

    my $data = shift;
    return _create_token(Bugzilla->user->id, 'session', $data);
}

sub CleanTokenTable {
    my $dbh = Bugzilla->dbh;
    $dbh->bz_lock_tables('tokens WRITE');
    $dbh->do('DELETE FROM tokens
              WHERE ' . $dbh->sql_to_days('NOW()') . ' - ' .
                        $dbh->sql_to_days('issuedate') . ' >= ?',
              undef, MAX_TOKEN_AGE);
    $dbh->bz_unlock_tables();
}

sub GenerateUniqueToken {
    # Generates a unique random token.  Uses generate_random_password 
    # for the tokens themselves and checks uniqueness by searching for
    # the token in the "tokens" table.  Gives up if it can't come up
    # with a token after about one hundred tries.
    my ($table, $column) = @_;

    my $token;
    my $duplicate = 1;
    my $tries = 0;
    $table ||= "tokens";
    $column ||= "token";

    my $dbh = Bugzilla->dbh;
    my $sth = $dbh->prepare("SELECT userid FROM $table WHERE $column = ?");

    while ($duplicate) {
        ++$tries;
        if ($tries > 100) {
            ThrowCodeError("token_generation_error");
        }
        $token = generate_random_password();
        $sth->execute($token);
        $duplicate = $sth->fetchrow_array;
    }
    return $token;
}

# Cancels a previously issued token and notifies the system administrator.
# This should only happen when the user accidentally makes a token request
# or when a malicious hacker makes a token request on behalf of a user.
sub Cancel {
    my ($token, $cancelaction, $vars) = @_;
    my $dbh = Bugzilla->dbh;
    my $template = Bugzilla->template;
    $vars ||= {};

    # Get information about the token being cancelled.
    trick_taint($token);
    my ($issuedate, $tokentype, $eventdata, $loginname) =
        $dbh->selectrow_array('SELECT ' . $dbh->sql_date_format('issuedate') . ',
                                      tokentype, eventdata, login_name
                                 FROM tokens
                            LEFT JOIN profiles
                                   ON tokens.userid = profiles.userid
                                WHERE token = ?',
                                undef, $token);

    # If we are cancelling the creation of a new user account, then there
    # is no entry in the 'profiles' table.
    $loginname ||= $eventdata;
    $vars->{'emailaddress'} = $loginname . Bugzilla->params->{'emailsuffix'};
    $vars->{'maintainer'} = Bugzilla->params->{'maintainer'};
    $vars->{'remoteaddress'} = $::ENV{'REMOTE_ADDR'};
    $vars->{'token'} = $token;
    $vars->{'tokentype'} = $tokentype;
    $vars->{'issuedate'} = $issuedate;
    $vars->{'eventdata'} = $eventdata;
    $vars->{'cancelaction'} = $cancelaction;

    # Notify the user via email about the cancellation.

    my $message;
    $template->process("account/cancel-token.txt.tmpl", $vars, \$message)
      || ThrowTemplateError($template->error());

    MessageToMTA($message);

    # Delete the token from the database.
    DeleteToken($token);
}

sub DeletePasswordTokens {
    my ($userid, $reason) = @_;
    my $dbh = Bugzilla->dbh;

    detaint_natural($userid);
    my $tokens = $dbh->selectcol_arrayref('SELECT token FROM tokens
                                           WHERE userid = ? AND tokentype = ?',
                                           undef, ($userid, 'password'));

    foreach my $token (@$tokens) {
        Bugzilla::Token::Cancel($token, $reason);
    }
}

# Returns an email change token if the user has one. 
sub HasEmailChangeToken {
    my $userid = shift;
    my $dbh = Bugzilla->dbh;

    my $token = $dbh->selectrow_array('SELECT token FROM tokens
                                       WHERE userid = ?
                                       AND (tokentype = ? OR tokentype = ?) ' .
                                       $dbh->sql_limit(1),
                                       undef, ($userid, 'emailnew', 'emailold'));
    return $token;
}

# Returns the userid, issuedate and eventdata for the specified token
sub GetTokenData {
    my ($token) = @_;
    my $dbh = Bugzilla->dbh;

    return unless defined $token;
    trick_taint($token);

    return $dbh->selectrow_array(
        "SELECT userid, " . $dbh->sql_date_format('issuedate') . ", eventdata 
         FROM   tokens 
         WHERE  token = ?", undef, $token);
}

# Deletes specified token
sub DeleteToken {
    my ($token) = @_;
    my $dbh = Bugzilla->dbh;

    return unless defined $token;
    trick_taint($token);

    $dbh->bz_lock_tables('tokens WRITE');
    $dbh->do("DELETE FROM tokens WHERE token = ?", undef, $token);
    $dbh->bz_unlock_tables();
}

################################################################################
# Internal Functions
################################################################################

# Generates a unique token and inserts it into the database
# Returns the token and the token timestamp
sub _create_token {
    my ($userid, $tokentype, $eventdata) = @_;
    my $dbh = Bugzilla->dbh;

    detaint_natural($userid);
    trick_taint($tokentype);
    trick_taint($eventdata);

    $dbh->bz_lock_tables('tokens WRITE');

    my $token = GenerateUniqueToken();

    $dbh->do("INSERT INTO tokens (userid, issuedate, token, tokentype, eventdata)
        VALUES (?, NOW(), ?, ?, ?)", undef, ($userid, $token, $tokentype, $eventdata));

    $dbh->bz_unlock_tables();

    if (wantarray) {
        my (undef, $token_ts, undef) = GetTokenData($token);
        $token_ts = str2time($token_ts);
        return ($token, $token_ts);
    } else {
        return $token;
    }
}

1;
