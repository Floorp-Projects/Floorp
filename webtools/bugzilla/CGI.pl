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

# Contains some global routines used throughout the CGI scripts of Bugzilla.

use strict;
use lib ".";

# use Carp;                       # for confess

use Bugzilla::Util;
use Bugzilla::Config;

# commented out the following snippet of code. this tosses errors into the
# CGI if you are perl 5.6, and doesn't if you have perl 5.003. 
# We want to check for the existence of the LDAP modules here.
# eval "use Mozilla::LDAP::Conn";
# my $have_ldap = $@ ? 0 : 1;

# Shut up misguided -w warnings about "used only once".  For some reason,
# "use vars" chokes on me when I try it here.

sub CGI_pl_sillyness {
    my $zz;
    $zz = %::dontchange;
}

use CGI::Carp qw(fatalsToBrowser);

require 'globals.pl';

use vars qw($template $vars);

# If Bugzilla is shut down, do not go any further, just display a message
# to the user about the downtime.  (do)editparams.cgi is exempted from
# this message, of course, since it needs to be available in order for
# the administrator to open Bugzilla back up.
if (Param("shutdownhtml") && $0 !~ m:[\\/](do)?editparams.cgi$:) {
    $::vars->{'message'} = "shutdown";
    
    # Return the appropriate HTTP response headers.
    print "Content-Type: text/html\n\n";
    
    # Generate and return an HTML message about the downtime.
    $::template->process("global/message.html.tmpl", $::vars)
      || ThrowTemplateError($::template->error());
    exit;
}

# Implementations of several of the below were blatently stolen from CGI.pm,
# by Lincoln D. Stein.

# Get rid of all the %xx encoding and the like from the given URL.
sub url_decode {
    my ($todecode) = (@_);
    $todecode =~ tr/+/ /;       # pluses become spaces
    $todecode =~ s/%([0-9a-fA-F]{2})/pack("c",hex($1))/ge;
    return $todecode;
}

# check and see if a given field exists, is non-empty, and is set to a 
# legal value.  assume a browser bug and abort appropriately if not.
# if $legalsRef is not passed, just check to make sure the value exists and 
# is non-NULL
sub CheckFormField (\%$;\@) {
    my ($formRef,                # a reference to the form to check (a hash)
        $fieldname,              # the fieldname to check
        $legalsRef               # (optional) ref to a list of legal values 
       ) = @_;

    if ( !defined $formRef->{$fieldname} ||
         trim($formRef->{$fieldname}) eq "" ||
         (defined($legalsRef) && 
          lsearch($legalsRef, $formRef->{$fieldname})<0) ){

        SendSQL("SELECT description FROM fielddefs WHERE name=" . SqlQuote($fieldname));
        my $result = FetchOneColumn();
        if ($result) {
            $vars->{'field'} = $result;
        }
        else {
            $vars->{'field'} = $fieldname;
        }
        
        ThrowCodeError("illegal_field", undef, "abort");
      }
}

# check and see if a given field is defined, and abort if not
sub CheckFormFieldDefined (\%$) {
    my ($formRef,                # a reference to the form to check (a hash)
        $fieldname,              # the fieldname to check
       ) = @_;

    if (!defined $formRef->{$fieldname}) {
        $vars->{'field'} = $fieldname;  
        ThrowCodeError("undefined_field");
    }
}

sub BugAliasToID {
    # Queries the database for the bug with a given alias, and returns
    # the ID of the bug if it exists or the undefined value if it doesn't.
    
    my ($alias) = @_;
    
    return undef unless Param("usebugaliases");
    
    PushGlobalSQLState();
    SendSQL("SELECT bug_id FROM bugs WHERE alias = " . SqlQuote($alias));
    my $id = FetchOneColumn();
    PopGlobalSQLState();
    
    return $id;
}

sub ValidateBugID {
    # Validates and verifies a bug ID, making sure the number is a 
    # positive integer, that it represents an existing bug in the
    # database, and that the user is authorized to access that bug.
    # We detaint the number here, too

    my ($id, $skip_authorization) = @_;
    
    # Get rid of white-space around the ID.
    $id = trim($id);
    
    # If the ID isn't a number, it might be an alias, so try to convert it.
    my $alias = $id;
    if (!detaint_natural($id)) {
        $id = BugAliasToID($alias);
        $id || ThrowUserError("invalid_bug_id_or_alias", {'bug_id' => $id});
    }
    
    # Modify the calling code's original variable to contain the trimmed,
    # converted-from-alias ID.
    $_[0] = $id;
    
    # First check that the bug exists
    SendSQL("SELECT bug_id FROM bugs WHERE bug_id = $id");

    FetchOneColumn()
      || ThrowUserError("invalid_bug_id_non_existent", {'bug_id' => $id});

    return if $skip_authorization;
    
    return if CanSeeBug($id, $::userid);

    # The user did not pass any of the authorization tests, which means they
    # are not authorized to see the bug.  Display an error and stop execution.
    # The error the user sees depends on whether or not they are logged in
    # (i.e. $::userid contains the user's positive integer ID).
    if ($::userid) {
        ThrowUserError("bug_access_denied", {'bug_id' => $id});
    } else {
        ThrowUserError("bug_access_query", {'bug_id' => $id});
    }
}

sub ValidateComment {
    # Make sure a comment is not too large (greater than 64K).
    
    my ($comment) = @_;
    
    if (defined($comment) && length($comment) > 65535) {
        ThrowUserError("comment_too_long");
    }
}

sub PasswordForLogin {
    my ($login) = (@_);
    SendSQL("select cryptpassword from profiles where login_name = " .
            SqlQuote($login));
    my $result = FetchOneColumn();
    if (!defined $result) {
        $result = "";
    }
    return $result;
}

sub get_netaddr {
    my ($ipaddr) = @_;

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

my $login_cookie_set = 0;
# If quietly_check_login is called with no arguments and logins are
# required, it will prompt for a login.
sub quietly_check_login {
    if (Param('requirelogin') && !(@_)) {
        return confirm_login();
    }
    $::disabledreason = '';
    my $userid = 0;
    my $ipaddr = $ENV{'REMOTE_ADDR'};
    my $netaddr = get_netaddr($ipaddr);
    if (defined $::COOKIE{"Bugzilla_login"} &&
        defined $::COOKIE{"Bugzilla_logincookie"}) {
        my $query = "SELECT profiles.userid," .
                " profiles.login_name, " .
                " profiles.disabledtext " .
                " FROM profiles, logincookies WHERE logincookies.cookie = " .
                SqlQuote($::COOKIE{"Bugzilla_logincookie"}) .
                " AND profiles.userid = logincookies.userid AND" .
                " profiles.login_name = " .
                SqlQuote($::COOKIE{"Bugzilla_login"}) .
                " AND (logincookies.ipaddr = " .
                SqlQuote($ipaddr);
        if (defined $netaddr) {
            $query .= " OR logincookies.ipaddr = " . SqlQuote($netaddr);
        }
        $query .= ")";
        SendSQL($query);

        my @row;
        if (MoreSQLData()) {
            ($userid, my $loginname, my $disabledtext) = FetchSQLData();
            if ($userid > 0) {
                if ($disabledtext eq '') {
                    $::COOKIE{"Bugzilla_login"} = $loginname; # Makes sure case
                                                              # is in
                                                              # canonical form.
                    # We've just verified that this is ok
                    detaint_natural($::COOKIE{"Bugzilla_logincookie"});
                } else {
                    $::disabledreason = $disabledtext;
                    $userid = 0;
                }
            }
        }
    }
    # if 'who' is passed in, verify that it's a good value
    if ($::FORM{'who'}) {
        my $whoid = DBname_to_id($::FORM{'who'});
        delete $::FORM{'who'} unless $whoid;
    }
    if (!$userid) {
        delete $::COOKIE{"Bugzilla_login"};
    }
                    
    $::userid = $userid;
    ConfirmGroup($userid);
    $vars->{'user'} = GetUserInfo($::userid);
    return $userid;
}

# Populate a hash with information about this user. 
sub GetUserInfo {
    my ($userid) = (@_);
    my %user;
    my @queries;
    my %groups;
    my @groupids;
    
    # No info if not logged in
    return \%user if ($userid == 0);
    
    $user{'login'} = $::COOKIE{"Bugzilla_login"};
    $user{'userid'} = $userid;
    
    SendSQL("SELECT mybugslink, realname " . 
            "FROM profiles WHERE userid = $userid");
    ($user{'showmybugslink'}, $user{'realname'}) = FetchSQLData();

    SendSQL("SELECT name, query, linkinfooter FROM namedqueries " .
            "WHERE userid = $userid");
    while (MoreSQLData()) {
        my %query;
        ($query{'name'}, $query{'query'}, $query{'linkinfooter'}) = 
                                                                 FetchSQLData();
        push(@queries, \%query);    
    }

    $user{'queries'} = \@queries;

    $user{'canblessany'} = UserCanBlessAnything();

    SendSQL("SELECT DISTINCT id, name FROM groups, user_group_map " .
            "WHERE groups.id = user_group_map.group_id " .
            "AND user_id = $userid " .
            "AND NOT isbless");
    while (MoreSQLData()) {
        my ($id, $name) = FetchSQLData();    
        push(@groupids,$id);
        $groups{$name} = 1;
    }

    $user{'groups'} = \%groups;
    $user{'groupids'} = \@groupids;

    return \%user;
}

sub CheckEmailSyntax {
    my ($addr) = (@_);
    my $match = Param('emailregexp');
    if ($addr !~ /$match/ || $addr =~ /[\\\(\)<>&,;:"\[\] \t\r\n]/) {
        $vars->{'addr'} = $addr;
        ThrowUserError("illegal_email_address");
    }
}

sub MailPassword {
    my ($login, $password) = (@_);
    my $urlbase = Param("urlbase");
    my $template = Param("passwordmail");
    my $msg = PerformSubsts($template,
                            {"mailaddress" => $login . Param('emailsuffix'),
                             "login" => $login,
                             "password" => $password});

    open SENDMAIL, "|/usr/lib/sendmail -t -i";
    print SENDMAIL $msg;
    close SENDMAIL;
}

sub confirm_login {
    my ($nexturl) = (@_);

# Uncommenting the next line can help debugging...
#    print "Content-type: text/plain\n\n";

    # I'm going to reorganize some of this stuff a bit.  Since we're adding
    # a second possible validation method (LDAP), we need to move some of this
    # to a later section.  -Joe Robins, 8/3/00
    my $enteredlogin = "";
    my $realcryptpwd = "";
    my $userid;

    # If the form contains Bugzilla login and password fields, use Bugzilla's 
    # built-in authentication to authenticate the user (otherwise use LDAP below).
    if (defined $::FORM{"Bugzilla_login"} && defined $::FORM{"Bugzilla_password"}) {
        # Make sure the user's login name is a valid email address.
        $enteredlogin = $::FORM{"Bugzilla_login"};
        CheckEmailSyntax($enteredlogin);

        # Retrieve the user's ID and crypted password from the database.
        SendSQL("SELECT userid, cryptpassword FROM profiles 
                 WHERE login_name = " . SqlQuote($enteredlogin));
        ($userid, $realcryptpwd) = FetchSQLData();

        # Make sure the user exists or throw an error (but do not admit it was a username
        # error to make it harder for a cracker to find account names by brute force).
        $userid || ThrowUserError("invalid_username_or_password");

        # If this is a new user, generate a password, insert a record
        # into the database, and email their password to them.
        if ( defined $::FORM{"PleaseMailAPassword"} && !$userid ) {
            # Ensure the new login is valid
            if(!ValidateNewUser($enteredlogin)) {
                ThrowUserError("account_exists");
            }

            my $password = InsertNewUser($enteredlogin, "");
            MailPassword($enteredlogin, $password);
            
            $vars->{'login'} = $enteredlogin;
            
            print "Content-Type: text/html\n\n";
            $template->process("account/created.html.tmpl", $vars)
              || ThrowTemplateError($template->error());                 
        }

        # Otherwise, authenticate the user.
        else {
            # Get the salt from the user's crypted password.
            my $salt = $realcryptpwd;

            # Using the salt, crypt the password the user entered.
            my $enteredCryptedPassword = crypt( $::FORM{"Bugzilla_password"} , $salt );

            # Make sure the passwords match or throw an error.
            ($enteredCryptedPassword eq $realcryptpwd)
              || ThrowUserError("invalid_username_or_password");

            # If the user has successfully logged in, delete any password tokens
            # lying around in the system for them.
            use Token;
            my $token = Token::HasPasswordToken($userid);
            while ( $token ) {
                Token::Cancel($token, "user logged in");
                $token = Token::HasPasswordToken($userid);
            }
        }

     } elsif (Param("useLDAP") &&
              defined $::FORM{"LDAP_login"} &&
              defined $::FORM{"LDAP_password"}) {
       # If we're using LDAP for login, we've got an entirely different
       # set of things to check.

# see comment at top of file near eval
       # First, if we don't have the LDAP modules available to us, we can't
       # do this.
#       if(!$have_ldap) {
#         print "Content-type: text/html\n\n";
#         PutHeader("LDAP not enabled");
#         print "The necessary modules for LDAP login are not installed on ";
#         print "this machine.  Please send mail to ".Param("maintainer");
#         print " and notify him of this problem.\n";
#         PutFooter();
#         exit;
#       }

       # Next, we need to bind anonymously to the LDAP server.  This is
       # because we need to get the Distinguished Name of the user trying
       # to log in.  Some servers (such as iPlanet) allow you to have unique
       # uids spread out over a subtree of an area (such as "People"), so
       # just appending the Base DN to the uid isn't sufficient to get the
       # user's DN.  For servers which don't work this way, there will still
       # be no harm done.
       my $LDAPserver = Param("LDAPserver");
       if ($LDAPserver eq "") {
         print "Content-type: text/html\n\n";
         PutHeader("LDAP server not defined");
         print "The LDAP server for authentication has not been defined.  ";
         print "Please contact ".Param("maintainer")." ";
         print "and notify him of this problem.\n";
         PutFooter();
         exit;
       }

       my $LDAPport = "389";  #default LDAP port
       if($LDAPserver =~ /:/) {
         ($LDAPserver, $LDAPport) = split(":",$LDAPserver);
       }
       my $LDAPconn = new Mozilla::LDAP::Conn($LDAPserver,$LDAPport);
       if(!$LDAPconn) {
         print "Content-type: text/html\n\n";
         PutHeader("Unable to connect to LDAP server");
         print "I was unable to connect to the LDAP server for user ";
         print "authentication.  Please contact ".Param("maintainer");
         print " and notify him of this problem.\n";
         PutFooter();
         exit;
       }

       # if no password was provided, then fail the authentication
       # while it may be valid to not have an LDAP password, when you
       # bind without a password (regardless of the binddn value), you
       # will get an anonymous bind.  I do not know of a way to determine
       # whether a bind is anonymous or not without making changes to the
       # LDAP access control settings
       if ( ! $::FORM{"LDAP_password"} ) {
         print "Content-type: text/html\n\n";
         PutHeader("Login Failed");
         print "You did not provide a password.\n";
         print "Please click <b>Back</b> and try again.\n";
         PutFooter();
         exit;
       }

       # We've got our anonymous bind;  let's look up this user.
       my $dnEntry = $LDAPconn->search(Param("LDAPBaseDN"),"subtree","uid=".$::FORM{"LDAP_login"});
       if(!$dnEntry) {
         print "Content-type: text/html\n\n";
         PutHeader("Login Failed");
         print "The username or password you entered is not valid.\n";
         print "Please click <b>Back</b> and try again.\n";
         PutFooter();
         exit;
       }

       # Now we get the DN from this search.  Once we've got that, we're
       # done with the anonymous bind, so we close it.
       my $userDN = $dnEntry->getDN;
       $LDAPconn->close;

       # Now we attempt to bind as the specified user.
       $LDAPconn = new Mozilla::LDAP::Conn($LDAPserver,$LDAPport,$userDN,$::FORM{"LDAP_password"});
       if(!$LDAPconn) {
         print "Content-type: text/html\n\n";
         PutHeader("Login Failed");
         print "The username or password you entered is not valid.\n";
         print "Please click <b>Back</b> and try again.\n";
         PutFooter();
         exit;
       }

       # And now we're going to repeat the search, so that we can get the
       # mail attribute for this user.
       my $userEntry = $LDAPconn->search(Param("LDAPBaseDN"),"subtree","uid=".$::FORM{"LDAP_login"});
       if(!$userEntry->exists(Param("LDAPmailattribute"))) {
         print "Content-type: text/html\n\n";
         PutHeader("LDAP authentication error");
         print "I was unable to retrieve the ".Param("LDAPmailattribute");
         print " attribute from the LDAP server.  Please contact ";
         print Param("maintainer")." and notify him of this error.\n";
         PutFooter();
         exit;
       }

       # Mozilla::LDAP::Entry->getValues returns an array for the attribute
       # requested, even if there's only one entry.
       $enteredlogin = ($userEntry->getValues(Param("LDAPmailattribute")))[0];

       # We're going to need the cryptpwd for this user from the database
       # so that we can set the cookie below, even though we're not going
       # to use it for authentication.
       $realcryptpwd = PasswordForLogin($enteredlogin);

       # If we don't get a result, then we've got a user who isn't in
       # Bugzilla's database yet, so we've got to add them.
       if($realcryptpwd eq "") {
         # We'll want the user's name for this.
         my $userRealName = ($userEntry->getValues("displayName"))[0];
         if($userRealName eq "") {
           $userRealName = ($userEntry->getValues("cn"))[0];
         }
         InsertNewUser($enteredlogin, $userRealName);
         $realcryptpwd = PasswordForLogin($enteredlogin);
       }
     } # end LDAP authentication

     # And now, if we've logged in via either method, then we need to set
     # the cookies.
     if($enteredlogin ne "") {
       $::COOKIE{"Bugzilla_login"} = $enteredlogin;
       my $ipaddr = $ENV{'REMOTE_ADDR'};

       # Unless we're restricting the login, or restricting would have no
       # effect, loosen the IP which we record in the table
       unless ($::FORM{'Bugzilla_restrictlogin'} ||
               Param('loginnetmask') == 32) {
           $ipaddr = get_netaddr($ipaddr);
           $ipaddr = $ENV{'REMOTE_ADDR'} unless defined $ipaddr;
       }
       SendSQL("insert into logincookies (userid,ipaddr) values (@{[DBNameToIdAndCheck($enteredlogin)]}, @{[SqlQuote($ipaddr)]})");
       SendSQL("select LAST_INSERT_ID()");
       my $logincookie = FetchOneColumn();

       $::COOKIE{"Bugzilla_logincookie"} = $logincookie;
       my $cookiepath = Param("cookiepath");
       if ($login_cookie_set == 0) {
           $login_cookie_set = 1;
           print "Set-Cookie: Bugzilla_login= " . url_quote($enteredlogin) . " ; path=$cookiepath; expires=Sun, 30-Jun-2029 00:00:00 GMT\n";
           print "Set-Cookie: Bugzilla_logincookie=$logincookie ; path=$cookiepath; expires=Sun, 30-Jun-2029 00:00:00 GMT\n";
       }
    }

    # If anonymous logins are disabled, quietly_check_login will force
    # the user to log in by calling confirm_login() when called by any 
    # code that does not call it with an argument. When confirm_login
    # calls quietly_check_login, it must not result in confirm_login
    # being called back.
    $userid = quietly_check_login('do_not_recurse_here');

    if (!$userid) {
        if ($::disabledreason) {
            my $cookiepath = Param("cookiepath");
            print "Set-Cookie: Bugzilla_login= ; path=$cookiepath; expires=Sun, 30-Jun-80 00:00:00 GMT
Set-Cookie: Bugzilla_logincookie= ; path=$cookiepath; expires=Sun, 30-Jun-80 00:00:00 GMT
Content-type: text/html

";
            $vars->{'disabled_reason'} = $::disabledreason;
            ThrowUserError("account_disabled");
        }
        
        if (!defined $nexturl || $nexturl eq "") {
            # Sets nexturl to be argv0, stripping everything up to and
            # including the last slash (or backslash on Windows).
            $0 =~ m:([^/\\]*)$:;
            $nexturl = $1;
        }
        
        $vars->{'target'} = $nexturl;
        $vars->{'form'} = \%::FORM;
        $vars->{'mform'} = \%::MFORM;
        
        print "Content-type: text/html\n\n";
        $template->process("account/login.html.tmpl", $vars)
          || ThrowTemplateError($template->error());
                
        # This seems like as good as time as any to get rid of old
        # crufty junk in the logincookies table.  Get rid of any entry
        # that hasn't been used in a month.
        if (Bugzilla->dbwritesallowed) {
            SendSQL("DELETE FROM logincookies " .
                    "WHERE TO_DAYS(NOW()) - TO_DAYS(lastused) > 30");
        }

        exit;
    }

    # Update the timestamp on our logincookie, so it'll keep on working.
    if (Bugzilla->dbwritesallowed) {
        SendSQL("UPDATE logincookies SET lastused = null " .
                "WHERE cookie = $::COOKIE{'Bugzilla_logincookie'}");
    }
    ConfirmGroup($userid);
    return $userid;
}

sub PutHeader {
    ($vars->{'title'}, $vars->{'h1'}, $vars->{'h2'}) = (@_);
     
    $::template->process("global/header.html.tmpl", $::vars)
      || ThrowTemplateError($::template->error());
    $vars->{'header_done'} = 1;
}

sub PutFooter {
    $::template->process("global/footer.html.tmpl", $::vars)
      || ThrowTemplateError($::template->error());
}

###############################################################################
# Error handling
#
# If you are doing incremental output, set $vars->{'header_done'} once you've
# done the header.
#
# You can call Throw*Error with extra template variables in one pass by using
# the $extra_vars hash reference parameter:
# ThrowUserError("some_tag", { bug_id => $bug_id, size => 127 });
###############################################################################

# For "this shouldn't happen"-type places in the code.
# The contents of $extra_vars get printed out in the template - useful for
# debugging info.
sub ThrowCodeError {
  ($vars->{'error'}, my $extra_vars, my $unlock_tables) = (@_);

  SendSQL("UNLOCK TABLES") if $unlock_tables;
  
  # Copy the extra_vars into the vars hash 
  foreach my $var (keys %$extra_vars) {
      $vars->{$var} = $extra_vars->{$var};
  }
  
  # We may one day log something to file here also.
  $vars->{'variables'} = $extra_vars;
  
  print "Content-type: text/html\n\n" if !$vars->{'header_done'};
  $template->process("global/code-error.html.tmpl", $vars)
    || ThrowTemplateError($template->error());
    
  exit;
}

# For errors made by the user.
sub ThrowUserError {
  ($vars->{'error'}, my $extra_vars, my $unlock_tables) = (@_);

  SendSQL("UNLOCK TABLES") if $unlock_tables;
 
  # Copy the extra_vars into the vars hash 
  foreach my $var (keys %$extra_vars) {
      $vars->{$var} = $extra_vars->{$var};
  }
  
  print "Content-type: text/html\n\n" if !$vars->{'header_done'};
  $template->process("global/user-error.html.tmpl", $vars)
    || ThrowTemplateError($template->error());
    
  exit;
}

# This function should only be called if a template->process() fails.
# It tries another template first, because often one template being
# broken or missing doesn't mean that they all are. But it falls back on
# a print statement.
# The Content-Type will already have been printed.
sub ThrowTemplateError {
    ($vars->{'template_error_msg'}) = (@_);
    $vars->{'error'} = "template_error";
    
    # Try a template first; but if this one fails too, fall back
    # on plain old print statements.
    if (!$template->process("global/code-error.html.tmpl", $vars)) {
        my $maintainer = Param('maintainer');
        my $error = html_quote($vars->{'template_error_msg'});
        my $error2 = html_quote($template->error());
        print <<END;
        <tt>
          <p>
            Bugzilla has suffered an internal error. Please save this page and 
            send it to $maintainer with details of what you were doing at the 
            time this message appeared.
          </p>
          <script type="text/javascript"> <!--
            document.write("<p>URL: " + document.location + "</p>");
          // -->
          </script>
          <p>Template->process() failed twice.<br>
          First error: $error<br>
          Second error: $error2</p>
        </tt>
END
    }
    
    exit;  
}

sub CheckIfVotedConfirmed {
    my ($id, $who) = (@_);
    SendSQL("SELECT bugs.votes, bugs.bug_status, products.votestoconfirm, " .
            "       bugs.everconfirmed " .
            "FROM bugs, products " .
            "WHERE bugs.bug_id = $id AND products.id = bugs.product_id");
    my ($votes, $status, $votestoconfirm, $everconfirmed) = (FetchSQLData());
    if ($votes >= $votestoconfirm && $status eq $::unconfirmedstate) {
        SendSQL("UPDATE bugs SET bug_status = 'NEW', everconfirmed = 1 " .
                "WHERE bug_id = $id");
        my $fieldid = GetFieldID("bug_status");
        SendSQL("INSERT INTO bugs_activity " .
                "(bug_id,who,bug_when,fieldid,removed,added) VALUES " .
                "($id,$who,now(),$fieldid,'$::unconfirmedstate','NEW')");
        if (!$everconfirmed) {
            $fieldid = GetFieldID("everconfirmed");
            SendSQL("INSERT INTO bugs_activity " .
                    "(bug_id,who,bug_when,fieldid,removed,added) VALUES " .
                    "($id,$who,now(),$fieldid,'0','1')");
        }
        
        AppendComment($id, DBID_to_name($who),
                      "*** This bug has been confirmed by popular vote. ***", 0);
                      
        $vars->{'type'} = "votes";
        $vars->{'id'} = $id;
        $vars->{'mail'} = "";
        open(PMAIL, "-|") or exec('./processmail', $id);
        $vars->{'mail'} .= $_ while <PMAIL>;
        close(PMAIL);
        
        $template->process("bug/process/results.html.tmpl", $vars)
          || ThrowTemplateError($template->error());
    }

}
sub LogActivityEntry {
    my ($i,$col,$removed,$added,$whoid,$timestamp) = @_;
    # in the case of CCs, deps, and keywords, there's a possibility that someone    # might try to add or remove a lot of them at once, which might take more
    # space than the activity table allows.  We'll solve this by splitting it
    # into multiple entries if it's too long.
    while ($removed || $added) {
        my ($removestr, $addstr) = ($removed, $added);
        if (length($removestr) > 254) {
            my $commaposition = FindWrapPoint($removed, 254);
            $removestr = substr($removed,0,$commaposition);
            $removed = substr($removed,$commaposition);
            $removed =~ s/^[,\s]+//; # remove any comma or space
        } else {
            $removed = ""; # no more entries
        }
        if (length($addstr) > 254) {
            my $commaposition = FindWrapPoint($added, 254);
            $addstr = substr($added,0,$commaposition);
            $added = substr($added,$commaposition);
            $added =~ s/^[,\s]+//; # remove any comma or space
        } else {
            $added = ""; # no more entries
        }
        $addstr = SqlQuote($addstr);
        $removestr = SqlQuote($removestr);
        my $fieldid = GetFieldID($col);
        SendSQL("INSERT INTO bugs_activity " .
                "(bug_id,who,bug_when,fieldid,removed,added) VALUES " .
                "($i,$whoid," . SqlQuote($timestamp) . ",$fieldid,$removestr,$addstr)");
    }
}

sub GetBugActivity {
    my ($id, $starttime) = (@_);
    my $datepart = "";

    die "Invalid id: $id" unless $id=~/^\s*\d+\s*$/;

    if (defined $starttime) {
        $datepart = "and bugs_activity.bug_when > " . SqlQuote($starttime);
    }
    
    my $query = "
        SELECT IFNULL(fielddefs.description, bugs_activity.fieldid),
                fielddefs.name,
                bugs_activity.attach_id,
                DATE_FORMAT(bugs_activity.bug_when,'%Y.%m.%d %H:%i'),
                bugs_activity.removed, bugs_activity.added,
                profiles.login_name
        FROM bugs_activity LEFT JOIN fielddefs ON 
                                     bugs_activity.fieldid = fielddefs.fieldid,
             profiles
        WHERE bugs_activity.bug_id = $id $datepart
              AND profiles.userid = bugs_activity.who
        ORDER BY bugs_activity.bug_when";

    SendSQL($query);
    
    my @operations;
    my $operation = {};
    my $changes = [];
    my $incomplete_data = 0;
    
    while (my ($field, $fieldname, $attachid, $when, $removed, $added, $who) 
                                                               = FetchSQLData())
    {
        my %change;
        my $activity_visible = 1;
        
        # check if the user should see this field's activity
        if ($fieldname eq 'remaining_time' ||
            $fieldname eq 'estimated_time' ||
            $fieldname eq 'work_time') {

            if (!UserInGroup(Param('timetrackinggroup'))) {
                $activity_visible = 0;
            } else {
                $activity_visible = 1;
            }
        } else {
            $activity_visible = 1;
        }
                
        if ($activity_visible) {
            # This gets replaced with a hyperlink in the template.
            $field =~ s/^Attachment// if $attachid;

            # Check for the results of an old Bugzilla data corruption bug
            $incomplete_data = 1 if ($added =~ /^\?/ || $removed =~ /^\?/);
        
            # An operation, done by 'who' at time 'when', has a number of
            # 'changes' associated with it.
            # If this is the start of a new operation, store the data from the
            # previous one, and set up the new one.
            if ($operation->{'who'} 
                && ($who ne $operation->{'who'} 
                    || $when ne $operation->{'when'})) 
            {
                $operation->{'changes'} = $changes;
                push (@operations, $operation);
            
                # Create new empty anonymous data structures.
                $operation = {};
                $changes = [];
            }  
        
            $operation->{'who'} = $who;
            $operation->{'when'} = $when;            
        
            $change{'field'} = $field;
            $change{'fieldname'} = $fieldname;
            $change{'attachid'} = $attachid;
            $change{'removed'} = $removed;
            $change{'added'} = $added;
            push (@$changes, \%change);
        }   
    }
    
    if ($operation->{'who'}) {
        $operation->{'changes'} = $changes;
        push (@operations, $operation);
    }
    
    return(\@operations, $incomplete_data);
}

############# Live code below here (that is, not subroutine defs) #############

use Bugzilla;

# XXX - mod_perl - reset this between runs
$::cgi = Bugzilla->cgi;

# Set up stuff for compatibility with the old CGI.pl code
# This code will be removed as soon as possible, in favour of
# using the CGI.pm stuff directly

# XXX - mod_perl - reset these between runs

foreach my $name ($::cgi->param()) {
    my @val = $::cgi->param($name);
    $::FORM{$name} = join('', @val);
    $::MFORM{$name} = \@val;
}

$::buffer = $::cgi->query_string();

foreach my $name ($::cgi->cookie()) {
    $::COOKIE{$name} = $::cgi->cookie($name);
}

# This could be needed in any CGI, so we set it here.
$vars->{'help'} = $::cgi->param('help') ? 1 : 0;

1;
