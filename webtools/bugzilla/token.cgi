#!/usr/bonsaitools/bin/perl -wT
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
# Contributor(s): Myk Melez <myk@mozilla.org>

############################################################################
# Script Initialization
############################################################################

# Make it harder for us to do dangerous things in Perl.
use strict;

use lib qw(.);

use vars qw($template $vars);

# Include the Bugzilla CGI and general utility library.
require "CGI.pl";

# Establish a connection to the database backend.
ConnectToDatabase();
quietly_check_login('permit_anonymous');

# Use the "Token" module that contains functions for doing various
# token-related tasks.
use Token;

################################################################################
# Data Validation / Security Authorization
################################################################################

# Throw an error if the form does not contain an "action" field specifying
# what the user wants to do.
$::FORM{'a'} || ThrowCodeError("unknown_action");

# Assign the action to a global variable.
$::action = $::FORM{'a'};

# If a token was submitted, make sure it is a valid token that exists in the
# database and is the correct type for the action being taken.
if ($::FORM{'t'}) {
  # Assign the token and its SQL quoted equivalent to global variables.
  $::token = $::FORM{'t'};
  $::quotedtoken = SqlQuote($::token);
  
  # Make sure the token contains only valid characters in the right amount.
  my $validationerror = ValidatePassword($::token);
  if ($validationerror) {
      ThrowUserError("token_invalid");
  }


  Token::CleanTokenTable();

  # Make sure the token exists in the database.
  SendSQL( "SELECT tokentype FROM tokens WHERE token = $::quotedtoken" );
  (my $tokentype = FetchSQLData()) || ThrowUserError("token_inexistent");

  # Make sure the token is the correct type for the action being taken.
  if ( grep($::action eq $_ , qw(cfmpw cxlpw chgpw)) && $tokentype ne 'password' ) {
    Token::Cancel($::token, "wrong_token_for_changing_passwd");
    ThrowUserError("wrong_token_for_changing_passwd");
  }
  if ( ($::action eq 'cxlem') 
      && (($tokentype ne 'emailold') && ($tokentype ne 'emailnew')) ) {
    Token::Cancel($::token, "wrong_token_for_cancelling_email_change");
    ThrowUserError("wrong_token_for_cancelling_email_change");
  }
  if ( grep($::action eq $_ , qw(cfmem chgem)) 
      && ($tokentype ne 'emailnew') ) {
    Token::Cancel($::token, "wrong_token_for_confirming_email_change");
    ThrowUserError("wrong_token_for_confirming_email_change");
  }
}

# If the user is requesting a password change, make sure they submitted
# their login name and it exists in the database.
if ( $::action eq 'reqpw' ) {
    defined $::FORM{'loginname'}
      || ThrowUserError("login_needed_for_password_change");

    # Make sure the login name looks like an email address.  This function
    # displays its own error and stops execution if the login name looks wrong.
    CheckEmailSyntax($::FORM{'loginname'});

    my $quotedloginname = SqlQuote($::FORM{'loginname'});
    SendSQL("SELECT userid FROM profiles WHERE login_name = $quotedloginname");
    FetchSQLData()
      || ThrowUserError("account_inexistent");
}

# If the user is changing their password, make sure they submitted a new
# password and that the new password is valid.
if ( $::action eq 'chgpw' ) {
    defined $::FORM{'password'}
      && defined $::FORM{'matchpassword'}
      || ThrowUserError("require_new_password");

    ValidatePassword($::FORM{'password'}, $::FORM{'matchpassword'});
}

################################################################################
# Main Body Execution
################################################################################

# All calls to this script should contain an "action" variable whose value
# determines what the user wants to do.  The code below checks the value of
# that variable and runs the appropriate code.

if ($::action eq 'reqpw') { 
    requestChangePassword(); 
} elsif ($::action eq 'cfmpw') { 
    confirmChangePassword(); 
} elsif ($::action eq 'cxlpw') { 
    cancelChangePassword(); 
} elsif ($::action eq 'chgpw') { 
    changePassword(); 
} elsif ($::action eq 'cfmem') {
    confirmChangeEmail();
} elsif ($::action eq 'cxlem') {
    cancelChangeEmail();
} elsif ($::action eq 'chgem') {
    changeEmail();
} else { 
    # If the action that the user wants to take (specified in the "a" form field)
    # is none of the above listed actions, display an error telling the user 
    # that we do not understand what they would like to do.
    ThrowCodeError("unknown_action");
}

exit;

################################################################################
# Functions
################################################################################

sub requestChangePassword {
    Token::IssuePasswordToken($::FORM{'loginname'});

    $vars->{'message'} = "password_change_request";

    print "Content-Type: text/html\n\n";
    $template->process("global/message.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

sub confirmChangePassword {
    $vars->{'token'} = $::token;
    
    print "Content-Type: text/html\n\n";
    $template->process("account/password/set-forgotten-password.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

sub cancelChangePassword {    
    $vars->{'message'} = "password_change_canceled";
    Token::Cancel($::token, $vars->{'message'});

    print "Content-Type: text/html\n\n";
    $template->process("global/message.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

sub changePassword {
    # Quote the password and token for inclusion into SQL statements.
    my $cryptedpassword = Crypt($::FORM{'password'});
    my $quotedpassword = SqlQuote($cryptedpassword);

    # Get the user's ID from the tokens table.
    SendSQL("SELECT userid FROM tokens WHERE token = $::quotedtoken");
    my $userid = FetchSQLData();
    
    # Update the user's password in the profiles table and delete the token
    # from the tokens table.
    SendSQL("LOCK TABLES profiles WRITE , tokens WRITE");
    SendSQL("UPDATE   profiles
             SET      cryptpassword = $quotedpassword
             WHERE    userid = $userid");
    SendSQL("DELETE FROM tokens WHERE token = $::quotedtoken");
    SendSQL("UNLOCK TABLES");

    InvalidateLogins($userid);

    $vars->{'message'} = "password_changed";

    print "Content-Type: text/html\n\n";
    $template->process("global/message.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

sub confirmChangeEmail {
    # Return HTTP response headers.
    print "Content-Type: text/html\n\n";

    $vars->{'token'} = $::token;

    $template->process("account/email/confirm.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

sub changeEmail {

    # Get the user's ID from the tokens table.
    SendSQL("SELECT userid, eventdata FROM tokens 
              WHERE token = $::quotedtoken");
    my ($userid, $eventdata) = FetchSQLData();
    my ($old_email, $new_email) = split(/:/,$eventdata);
    my $quotednewemail = SqlQuote($new_email);

    # Check the user entered the correct old email address
    if($::FORM{'email'} ne $old_email) {
        ThrowUserError("email_confirmation_failed");
    }
    # The new email address should be available as this was 
    # confirmed initially so cancel token if it is not still available
    if (! ValidateNewUser($new_email,$old_email)) {
        $vars->{'email'} = $new_email;
        Token::Cancel($::token,"account_exists");
        ThrowUserError("account_exists");
    } 

    # Update the user's login name in the profiles table and delete the token
    # from the tokens table.
    SendSQL("LOCK TABLES profiles WRITE , tokens WRITE");
    SendSQL("UPDATE   profiles
         SET      login_name = $quotednewemail
         WHERE    userid = $userid");
    SendSQL("DELETE FROM tokens WHERE token = $::quotedtoken");
    SendSQL("DELETE FROM tokens WHERE userid = $userid 
                                  AND tokentype = 'emailnew'");
    SendSQL("UNLOCK TABLES");
    DeriveGroup($userid);

    # Return HTTP response headers.
    print "Content-Type: text/html\n\n";

    # Let the user know their email address has been changed.

    $vars->{'message'} = "login_changed";

    $template->process("global/message.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

sub cancelChangeEmail {
    # Get the user's ID from the tokens table.
    SendSQL("SELECT userid, tokentype, eventdata FROM tokens 
             WHERE token = $::quotedtoken");
    my ($userid, $tokentype, $eventdata) = FetchSQLData();
    my ($old_email, $new_email) = split(/:/,$eventdata);

    if($tokentype eq "emailold") {
        $vars->{'message'} = "emailold_change_cancelled";

        SendSQL("SELECT login_name FROM profiles WHERE userid = $userid");
        my $actualemail = FetchSQLData();
        
        # check to see if it has been altered
        if($actualemail ne $old_email) {
            my $quotedoldemail = SqlQuote($old_email);

            SendSQL("LOCK TABLES profiles WRITE");
            SendSQL("UPDATE   profiles
                 SET      login_name = $quotedoldemail
                 WHERE    userid = $userid");
            SendSQL("UNLOCK TABLES");
            DeriveGroup($userid);
            $vars->{'message'} = "email_change_cancelled_reinstated";
        } 
    } 
    else {
        $vars->{'message'} = 'email_change_cancelled'
     }

    $vars->{'old_email'} = $old_email;
    $vars->{'new_email'} = $new_email;
    Token::Cancel($::token, $vars->{'message'});

    SendSQL("LOCK TABLES tokens WRITE");
    SendSQL("DELETE FROM tokens 
             WHERE userid = $userid 
             AND tokentype = 'emailold' OR tokentype = 'emailnew'");
    SendSQL("UNLOCK TABLES");

    # Return HTTP response headers.
    print "Content-Type: text/html\n\n";

    $template->process("global/message.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

