#!/usr/bonsaitools/bin/perl -w
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
use diagnostics;
use strict;

# Bundle the functions in this file together into the "Token" package.
package Token;

# This module requires that its caller have said "require CGI.pl" to import
# relevant functions from that script and its companion globals.pl.

################################################################################
# Functions
################################################################################

sub IssuePasswordToken {
    # Generates a random token, adds it to the tokens table, and sends it
    # to the user with instructions for using it to change their password.

    my ($loginname) = @_;

    # Retrieve the user's ID from the database.
    my $quotedloginname = &::SqlQuote($loginname);
    &::SendSQL("SELECT userid FROM profiles WHERE login_name = $quotedloginname");
    my ($userid) = &::FetchSQLData();

    # Generate a unique token and insert it into the tokens table.
    # We have to lock the tokens table before generating the token, 
    # since the database must be queried for token uniqueness.
    &::SendSQL("LOCK TABLES tokens WRITE");
    my $token = GenerateUniqueToken();
    my $quotedtoken = &::SqlQuote($token);
    my $quotedipaddr = &::SqlQuote($::ENV{'REMOTE_ADDR'});
    &::SendSQL("INSERT INTO tokens ( userid , issuedate , token , tokentype , eventdata )
                VALUES      ( $userid , NOW() , $quotedtoken , 'password' , $quotedipaddr )");
    &::SendSQL("UNLOCK TABLES");

    # Mail the user the token along with instructions for using it.
    MailPasswordToken($loginname, $token);

}


sub GenerateUniqueToken {
    # Generates a unique random token.  Uses &GenerateRandomPassword 
    # for the tokens themselves and checks uniqueness by searching for
    # the token in the "tokens" table.  Gives up if it can't come up
    # with a token after about one hundred tries.

    my $token;
    my $duplicate = 1;
    my $tries = 0;
    while ($duplicate) {

        ++$tries;
        if ($tries > 100) {
            &::DisplayError("Something is seriously wrong with the token generation system.");
            exit;
        }

        $token = &::GenerateRandomPassword();
        &::SendSQL("SELECT userid FROM tokens WHERE token = " . &::SqlQuote($token));
        $duplicate = &::FetchSQLData();
    }

    return $token;

}

sub MailPasswordToken {
    # Emails a password token to a user along with instructions for its use.
    # Called exclusively from &IssuePasswordToken.

    my ($emailaddress, $token) = @_;

    my $urlbase = &::Param("urlbase");
    my $emailsuffix = &::Param('emailsuffix');
    $token = &::url_quote($token);

    open SENDMAIL, "|/usr/lib/sendmail -t";

    print SENDMAIL qq|From: bugzilla-daemon
To: $emailaddress$emailsuffix
Subject: Bugzilla Change Password Request

You or someone impersonating you has requested to change your Bugzilla
password.  To change your password, visit the following link:

${urlbase}token.cgi?a=cfmpw&t=$token

If you are not the person who made this request, or you wish to cancel
this request, visit the following link:

${urlbase}token.cgi?a=cxlpw&t=$token
|;
    close SENDMAIL;
}

sub Cancel {
    # Cancels a previously issued token and notifies the system administrator.
    # This should only happen when the user accidentally makes a token request
    # or when a malicious hacker makes a token request on behalf of a user.
    
    my ($token, $cancelaction) = @_;

    # Quote the token for inclusion in SQL statements.
    my $quotedtoken = &::SqlQuote($token);
    
    # Get information about the token being cancelled.
    &::SendSQL("SELECT  issuedate , tokentype , eventdata , login_name , realname
                FROM    tokens, profiles 
                WHERE   tokens.userid = profiles.userid
                AND     token = $quotedtoken");
    my ($issuedate, $tokentype, $eventdata, $loginname, $realname) = &::FetchSQLData();

    # Get the email address of the Bugzilla maintainer.
    my $maintainer = &::Param('maintainer');

    # Format the user's real name and email address into a single string.
    my $username = $realname ? $realname . " <" . $loginname . ">" : $loginname;

    # Notify the user via email about the cancellation.
    open SENDMAIL, "|/usr/lib/sendmail -t";
    print SENDMAIL qq|From: bugzilla-daemon
To: $username
Subject: "$tokentype" token cancelled

A token was cancelled from $::ENV{'REMOTE_ADDR'}.  This is either 
an honest mistake or the result of a malicious hack attempt.  
Take a look at the information below and forward this email 
to $maintainer if you suspect foul play.

            Token: $token
       Token Type: $tokentype
             User: $username
       Issue Date: $issuedate
       Event Data: $eventdata

Cancelled Because: $cancelaction
|;
    close SENDMAIL;

    # Delete the token from the database.
    &::SendSQL("LOCK TABLES tokens WRITE");
    &::SendSQL("DELETE FROM tokens WHERE token = $quotedtoken");
    &::SendSQL("UNLOCK TABLES");
}

sub HasPasswordToken {
    # Returns a password token if the user has one.  Otherwise returns 0 (false).
    
    my ($userid) = @_;
    
    &::SendSQL("SELECT token FROM tokens WHERE userid = $userid LIMIT 1");
    my ($token) = &::FetchSQLData();
    
    return $token;
}

1;
