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

use diagnostics;
use strict;
use lib ".";

# use Carp;                       # for confess

# commented out the following snippet of code. this tosses errors into the
# CGI if you are perl 5.6, and doesn't if you have perl 5.003. 
# We want to check for the existence of the LDAP modules here.
# eval "use Mozilla::LDAP::Conn";
# my $have_ldap = $@ ? 0 : 1;

# Shut up misguided -w warnings about "used only once".  For some reason,
# "use vars" chokes on me when I try it here.

sub CGI_pl_sillyness {
    my $zz;
    $zz = %::MFORM;
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
    # The shut down message we are going to display to the user.
    $::vars->{'title'} = "Bugzilla is Down";
    $::vars->{'h1'} = "Bugzilla is Down";
    $::vars->{'message'} = Param("shutdownhtml");
    
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

# Quotify a string, suitable for putting into a URL.
sub url_quote {
    my($toencode) = (@_);
    $toencode=~s/([^a-zA-Z0-9_\-.])/uc sprintf("%%%02x",ord($1))/eg;
    return $toencode;
}

sub ParseUrlString {
    my ($buffer, $f, $m) = (@_);
    undef %$f;
    undef %$m;

    my %isnull;

    # We must make sure that the CGI params remain tainted.
    # This means that if for some reason you want to make this code
    # use a regexp and $1, $2, ... (or use a helper function which does so)
    # you must |use re 'taint'| _and_ make sure that you don't run into
    # http://bugs.perl.org/perlbug.cgi?req=bug_id&bug_id=20020704.001
    my @args = split('&', $buffer);
    foreach my $arg (@args) {
        my ($name, $value) = split('=', $arg, 2);
        $value = '' if not defined $value;

        $name = url_decode($name);
        $value = url_decode($value);

        if ($value ne "") {
            if (defined $f->{$name}) {
                $f->{$name} .= $value;
                my $ref = $m->{$name};
                push @$ref, $value;
            } else {
                $f->{$name} = $value;
                $m->{$name} = [$value];
            }
        } else {
            $isnull{$name} = 1;
        }
    }
    if (%isnull) {
        foreach my $name (keys(%isnull)) {
            if (!defined $f->{$name}) {
                $f->{$name} = "";
                $m->{$name} = [];
            }
        }
    }
}

sub ProcessFormFields {
    my ($buffer) = (@_);
    return ParseUrlString($buffer, \%::FORM, \%::MFORM);
}

sub ProcessMultipartFormFields {
    my ($boundary) = @_;

    # Initialize variables that store whether or not we are parsing a header,
    # the name of the part we are parsing, and its value (which is incomplete
    # until we finish parsing the part).
    my $inheader = 1;
    my $fieldname = "";
    my $fieldvalue = "";

    # Read the input stream line by line and parse it into a series of parts,
    # each one containing a single form field and its value and each one
    # separated from the next by the value of $boundary.
    my $remaining = $ENV{"CONTENT_LENGTH"};
    while ($remaining > 0 && ($_ = <STDIN>)) {
        $remaining -= length($_);

        # If the current input line is a boundary line, save the previous
        # form value and reset the storage variables.
        if ($_ =~ m/^-*\Q$boundary\E/) {
            if ( $fieldname ) {
                chomp($fieldvalue);
                $fieldvalue =~ s/\r$//;
                if ( defined $::FORM{$fieldname} ) {
                    $::FORM{$fieldname} .= $fieldvalue;
                    push @{$::MFORM{$fieldname}}, $fieldvalue;
                } else {
                    $::FORM{$fieldname} = $fieldvalue;
                    $::MFORM{$fieldname} = [$fieldvalue];
                }
            }

            $inheader = 1;
            $fieldname = "";
            $fieldvalue = "";

        # If the current input line is a header line, look for a blank line
        # (meaning the end of the headers), a Content-Disposition header
        # (containing the field name and, for uploaded file parts, the file 
        # name), or a Content-Type header (containing the content type for 
        # file parts).
        } elsif ( $inheader ) {
            if (m/^\s*$/) {
                $inheader = 0;
            } elsif (m/^Content-Disposition:\s*form-data\s*;\s*name\s*=\s*"([^\"]+)"/i) {
                $fieldname = $1;
                if (m/;\s*filename\s*=\s*"([^\"]+)"/i) {
                    $::FILE{$fieldname}->{'filename'} = $1;
                }
            } elsif ( m|^Content-Type:\s*([^/]+/[^\s;]+)|i ) {
                $::FILE{$fieldname}->{'contenttype'} = $1;
            }

        # If the current input line is neither a boundary line nor a header,
        # it must be part of the field value, so append it to the value.
        } else {
          $fieldvalue .= $_;
        }
    }
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
        
        ThrowCodeError("illegal_field", "abort");
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
        if (!$id) {
            my $html_id = html_quote($_[0]);
            my $alias_specific_message = Param("usebugaliases") ? 
              " (it is neither a bug number nor an alias to a bug number)" : "";
            DisplayError(qq|
              The bug number <em>$html_id</em> is invalid$alias_specific_message.
              If you are trying to use QuickSearch, you need to enable JavaScript 
              in your browser. To help us fix this limitation, add your comments 
              to <a href="http://bugzilla.mozilla.org/show_bug.cgi?id=70907">bug 
              70907</a>.
            |);
            exit;
        }
    }
    
    # Modify the calling code's original variable to contain the trimmed,
    # converted-from-alias ID.
    $_[0] = $id;
    
    # Get the values of the usergroupset and userid global variables
    # and write them to local variables for use within this function,
    # setting those local variables to the default value of zero if
    # the global variables are undefined.

    # First check that the bug exists
    SendSQL("SELECT bug_id FROM bugs WHERE bug_id = $id");

    FetchOneColumn()
      || DisplayError("Bug #$id does not exist.")
        && exit;

    return if $skip_authorization;
    
    return if CanSeeBug($id, $::userid, $::usergroupset);

    # The user did not pass any of the authorization tests, which means they
    # are not authorized to see the bug.  Display an error and stop execution.
    # The error the user sees depends on whether or not they are logged in
    # (i.e. $::userid contains the user's positive integer ID).
    if ($::userid) {
        DisplayError("You are not authorized to access bug #$id.");
    } else {
        DisplayError(
          qq|You are not authorized to access bug #$id.  To see this bug, you
          must first <a href="show_bug.cgi?id=$id&amp;GoAheadAndLogIn=1">log in 
          to an account</a> with the appropriate permissions.|
        );
    }
    exit;

}

sub ValidateComment {
    # Make sure a comment is not too large (greater than 64K).
    
    my ($comment) = @_;
    
    if (defined($comment) && length($comment) > 65535) {
        DisplayError("Comments cannot be longer than 65,535 characters.");
        exit;
    }
}

sub html_quote {
    my ($var) = (@_);
    $var =~ s/\&/\&amp;/g;
    $var =~ s/</\&lt;/g;
    $var =~ s/>/\&gt;/g;
    $var =~ s/"/\&quot;/g;
    return $var;
}

sub value_quote {
    my ($var) = (@_);
    $var =~ s/\&/\&amp;/g;
    $var =~ s/</\&lt;/g;
    $var =~ s/>/\&gt;/g;
    $var =~ s/"/\&quot;/g;
    # See bug http://bugzilla.mozilla.org/show_bug.cgi?id=4928 for 
    # explanaion of why bugzilla does this linebreak substitution. 
    # This caused form submission problems in mozilla (bug 22983, 32000).
    $var =~ s/\r\n/\&#013;/g;
    $var =~ s/\n\r/\&#013;/g;
    $var =~ s/\r/\&#013;/g;
    $var =~ s/\n/\&#013;/g;
    return $var;
}

# Adds <link> elements for bug lists. These can be inserted into the header by
# using the "header_html" parameter to PutHeader, which inserts an arbitrary
# string into the header. This function is currently used only in
# template/en/default/bug/edit.html.tmpl.
sub navigation_links($) {
    my ($buglist) = @_;
    
    my $retval = "";
    
    # We need to be able to pass in a buglist because when you sort on a column
    # the bugs in the cookie you are given will still be in the old order.
    # If a buglist isn't passed, we just use the cookie.
    $buglist ||= $::COOKIE{"BUGLIST"};
    
    if (defined $buglist && $buglist ne "") {
    my @bugs = split(/:/, $buglist);
        
        if (defined $::FORM{'id'}) {
            # We are on an individual bug
            my $cur = lsearch(\@bugs, $::FORM{"id"});

            if ($cur > 0) {
                $retval .= "<link rel=\"First\" href=\"show_bug.cgi?id=$bugs[0]\">\n";
                $retval .= "<link rel=\"Prev\" href=\"show_bug.cgi?id=$bugs[$cur - 1]\">\n";
            } 
            if ($cur < $#bugs) {
                $retval .= "<link rel=\"Next\" href=\"show_bug.cgi?id=$bugs[$cur + 1]\">\n";
                $retval .= "<link rel=\"Last\" href=\"show_bug.cgi?id=$bugs[$#bugs]\">\n";
            }

            $retval .= "<link rel=\"Up\" href=\"buglist.cgi?regetlastlist=1\">\n";
            $retval .= "<link rel=\"Contents\" href=\"buglist.cgi?regetlastlist=1\">\n";
        } else {
            # We are on a bug list
            $retval .= "<link rel=\"First\" href=\"show_bug.cgi?id=$bugs[0]\">\n";
            $retval .= "<link rel=\"Next\" href=\"show_bug.cgi?id=$bugs[0]\">\n";
            $retval .= "<link rel=\"Last\" href=\"show_bug.cgi?id=$bugs[$#bugs]\">\n";
        }
    }
    
    return $retval;
} 

$::CheckOptionValues = 1;

# This sub is still used in reports.cgi.
sub make_options {
    my ($src,$default,$isregexp) = (@_);
    my $last = "";
    my $popup = "";
    my $found = 0;
    $default = "" if !defined $default;

    if ($src) {
        foreach my $item (@$src) {
            if ($item eq "-blank-" || $item ne $last) {
                if ($item eq "-blank-") {
                    $item = "";
                }
                $last = $item;
                if ($isregexp ? $item =~ $default : $default eq $item) {
                    $popup .= "<OPTION SELECTED VALUE=\"$item\">$item\n";
                    $found = 1;
                } else {
                    $popup .= "<OPTION VALUE=\"$item\">$item\n";
                }
            }
        }
    }
    if (!$found && $default ne "") {
      if ( $::CheckOptionValues &&
           ($default ne $::dontchange) && ($default ne "-All-") &&
           ($default ne "DUPLICATE") ) {
        print "Possible bug database corruption has been detected.  " .
              "Please send mail to " . Param("maintainer") . " with " .
              "details of what you were doing when this message " . 
              "appeared.  Thank you.\n";
        if (!$src) {
            $src = ["???null???"];
        }
        print "<pre>src = " . value_quote(join(' ', @$src)) . "\n";
        print "default = " . value_quote($default) . "</pre>";
        PutFooter();
#        confess "Gulp.";
        exit 0;
              
      } else {
        $popup .= "<OPTION SELECTED>$default\n";
      }
    }
    return $popup;
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

sub quietly_check_login() {
    $::usergroupset = '0';
    my $loginok = 0;
    $::disabledreason = '';
    $::userid = 0;
    if (defined $::COOKIE{"Bugzilla_login"} &&
        defined $::COOKIE{"Bugzilla_logincookie"}) {
        SendSQL("SELECT profiles.userid, profiles.groupset, " .
                "profiles.login_name, " .
                "profiles.login_name = " .
                SqlQuote($::COOKIE{"Bugzilla_login"}) .
                " AND logincookies.ipaddr = " .
                SqlQuote($ENV{"REMOTE_ADDR"}) .
                ", profiles.disabledtext " .
                " FROM profiles, logincookies WHERE logincookies.cookie = " .
                SqlQuote($::COOKIE{"Bugzilla_logincookie"}) .
                " AND profiles.userid = logincookies.userid");
        my @row;
        if (@row = FetchSQLData()) {
            my ($userid, $groupset, $loginname, $ok, $disabledtext) = (@row);
            if ($ok) {
                if ($disabledtext eq '') {
                    $loginok = 1;
                    $::userid = $userid;
                    $::usergroupset = $groupset;
                    $::COOKIE{"Bugzilla_login"} = $loginname; # Makes sure case
                                                              # is in
                                                              # canonical form.
                    # We've just verified that this is ok
                    detaint_natural($::COOKIE{"Bugzilla_logincookie"});
                } else {
                    $::disabledreason = $disabledtext;
                }
            }
        }
    }
    # if 'who' is passed in, verify that it's a good value
    if ($::FORM{'who'}) {
        my $whoid = DBname_to_id($::FORM{'who'});
        delete $::FORM{'who'} unless $whoid;
    }
    if (!$loginok) {
        delete $::COOKIE{"Bugzilla_login"};
    }
                    
    $vars->{'user'} = GetUserInfo($::userid);
    
    return $loginok;
}

# Populate a hash with information about this user. 
sub GetUserInfo {
    my ($userid) = (@_);
    my %user;
    my @queries;
    my %groups;
    
    # No info if not logged in
    return \%user if ($userid == 0);
    
    $user{'login'} = $::COOKIE{"Bugzilla_login"};
    $user{'userid'} = $userid;
    
    SendSQL("SELECT mybugslink, realname, groupset, blessgroupset " . 
            "FROM profiles WHERE userid = $userid");
    ($user{'showmybugslink'}, $user{'realname'}, $user{'groupset'},
                                       $user{'blessgroupset'}) = FetchSQLData();

    SendSQL("SELECT name, query, linkinfooter FROM namedqueries " .
            "WHERE userid = $userid");
    while (MoreSQLData()) {
        my %query;
        ($query{'name'}, $query{'query'}, $query{'linkinfooter'}) = 
                                                                 FetchSQLData();
        push(@queries, \%query);    
    }

    $user{'queries'} = \@queries;

    SendSQL("select name, (bit & $user{'groupset'}) != 0 from groups");
    while (MoreSQLData()) {
        my ($name, $bit) = FetchSQLData();    
        $groups{$name} = $bit;
    }

    $user{'groups'} = \%groups;

    return \%user;
}

sub CheckEmailSyntax {
    my ($addr) = (@_);
    my $match = Param('emailregexp');
    if ($addr !~ /$match/ || $addr =~ /[\\\(\)<>&,;:"\[\] \t\r\n]/) {
        ThrowUserError("The e-mail address you entered(<b>" .
        html_quote($addr) . "</b>) didn't pass our syntax checking 
        for a legal email address. " . Param('emailregexpdesc') .
        ' It must also not contain any of these special characters:
        <tt>\ ( ) &amp; &lt; &gt; , ; : " [ ]</tt>, or any whitespace.', 
        "Check e-mail address syntax");
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

    # If the form contains Bugzilla login and password fields, use Bugzilla's 
    # built-in authentication to authenticate the user (otherwise use LDAP below).
    if (defined $::FORM{"Bugzilla_login"} && defined $::FORM{"Bugzilla_password"}) {
        # Make sure the user's login name is a valid email address.
        $enteredlogin = $::FORM{"Bugzilla_login"};
        CheckEmailSyntax($enteredlogin);

        # Retrieve the user's ID and crypted password from the database.
        my $userid;
        SendSQL("SELECT userid, cryptpassword FROM profiles 
                 WHERE login_name = " . SqlQuote($enteredlogin));
        ($userid, $realcryptpwd) = FetchSQLData();

        # Make sure the user exists or throw an error (but do not admit it was a username
        # error to make it harder for a cracker to find account names by brute force).
        $userid
          || DisplayError("The username or password you entered is not valid.")
          && exit;

        # If this is a new user, generate a password, insert a record
        # into the database, and email their password to them.
        if ( defined $::FORM{"PleaseMailAPassword"} && !$userid ) {
            # Ensure the new login is valid
            if(!ValidateNewUser($enteredlogin)) {
                ThrowUserError("That account already exists.");
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
              || DisplayError("The username or password you entered is not valid.")
              && exit;

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
       SendSQL("insert into logincookies (userid,ipaddr) values (@{[DBNameToIdAndCheck($enteredlogin)]}, @{[SqlQuote($ENV{'REMOTE_ADDR'})]})");
       SendSQL("select LAST_INSERT_ID()");
       my $logincookie = FetchOneColumn();

       $::COOKIE{"Bugzilla_logincookie"} = $logincookie;
       my $cookiepath = Param("cookiepath");
       print "Set-Cookie: Bugzilla_login=$enteredlogin ; path=$cookiepath; expires=Sun, 30-Jun-2029 00:00:00 GMT\n";
       print "Set-Cookie: Bugzilla_logincookie=$logincookie ; path=$cookiepath; expires=Sun, 30-Jun-2029 00:00:00 GMT\n";
    }

    my $loginok = quietly_check_login();

    if ($loginok != 1) {
        if ($::disabledreason) {
            my $cookiepath = Param("cookiepath");
            print "Set-Cookie: Bugzilla_login= ; path=$cookiepath; expires=Sun, 30-Jun-80 00:00:00 GMT
Set-Cookie: Bugzilla_logincookie= ; path=$cookiepath; expires=Sun, 30-Jun-80 00:00:00 GMT
Content-type: text/html

";
            ThrowUserError($::disabledreason . "<hr>" .
            "If you believe your account should be restored, please " .
            "send email to " . Param("maintainer") . " explaining why.",
            "Your account has been disabled");
        }
        
        if (!defined $nexturl || $nexturl eq "") {
            # Sets nexturl to be argv0, stripping everything up to and
            # including the last slash (or backslash on Windows).
            $0 =~ m:([^/\\]*)$:;
            $nexturl = $1;
        }
        
        $vars->{'target'} = $nexturl;
        $vars->{'form'} = \%::FORM;
        
        print "Content-type: text/html\n\n";
        $template->process("account/login.html.tmpl", $vars)
          || ThrowTemplateError($template->error());
                
        # This seems like as good as time as any to get rid of old
        # crufty junk in the logincookies table.  Get rid of any entry
        # that hasn't been used in a month.
        if ($::dbwritesallowed) {
            SendSQL("DELETE FROM logincookies " .
                    "WHERE TO_DAYS(NOW()) - TO_DAYS(lastused) > 30");
        }

        exit;
    }

    # Update the timestamp on our logincookie, so it'll keep on working.
    if ($::dbwritesallowed) {
        SendSQL("UPDATE logincookies SET lastused = null " .
                "WHERE cookie = $::COOKIE{'Bugzilla_logincookie'}");
    }
    return $::userid;
}

sub PutHeader {
    ($vars->{'title'}, $vars->{'h1'}, $vars->{'h2'}) = (@_);
     
    $::template->process("global/header.html.tmpl", $::vars)
      || ThrowTemplateError($::template->error());
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
###############################################################################

# DisplayError is deprecated. Use ThrowCodeError, ThrowUserError or 
# ThrowTemplateError instead.
sub DisplayError {
  ($vars->{'error'}, $vars->{'title'}) = (@_);

  print "Content-type: text/html\n\n" if !$vars->{'header_done'};
  $template->process("global/user-error.html.tmpl", $vars)
    || ThrowTemplateError($template->error());   

  return 1;
}

# For "this shouldn't happen"-type places in the code.
# $vars->{'variables'} is a reference to a hash of useful debugging info.
sub ThrowCodeError {
  ($vars->{'error'}, my $unlock_tables, $vars->{'variables'}) = (@_);

  SendSQL("UNLOCK TABLES") if $unlock_tables;
  
  # We may one day log something to file here.
  
  print "Content-type: text/html\n\n" if !$vars->{'header_done'};
  $template->process("global/code-error.html.tmpl", $vars)
    || ThrowTemplateError($template->error());
    
  exit;
}

# For errors made by the user.
# The correct use of this function is to pass an error tag, defined in
# user-error.html.tmpl, as the first parameter, and then, optionally,
# undef as the second parameter and $unlock_tables as the third.
# The second parameter will eventually go away.
sub ThrowUserError {
  ($vars->{'error'}, $vars->{'title'}, my $unlock_tables) = (@_);

  SendSQL("UNLOCK TABLES") if $unlock_tables;
  
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
            "WHERE bugs.bug_id = $id AND products.product_id = bugs.product_id");
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
                      "*** This bug has been confirmed by popular vote. ***");
                      
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

sub GetBugActivity {
    my ($id, $starttime) = (@_);
    my $datepart = "";

    die "Invalid id: $id" unless $id=~/^\s*\d+\s*$/;

    if (defined $starttime) {
        $datepart = "and bugs_activity.bug_when > " . SqlQuote($starttime);
    }
    
    my $query = "
        SELECT IFNULL(fielddefs.description, bugs_activity.fieldid),
                bugs_activity.attach_id,
                bugs_activity.bug_when,
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
    
    while (my ($field, $attachid, $when, $removed, $added, $who) 
                                                               = FetchSQLData())
    {
        my %change;
        
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
        $change{'attachid'} = $attachid;
        $change{'removed'} = $removed;
        $change{'added'} = $added;
        push (@$changes, \%change);
    }
    
    if ($operation->{'who'}) {
        $operation->{'changes'} = $changes;
        push (@operations, $operation);
    }
    
    return(\@operations, $incomplete_data);
}


############# Live code below here (that is, not subroutine defs) #############

$| = 1;

# Uncommenting this next line can help debugging.
# print "Content-type: text/html\n\nHello mom\n";

# foreach my $k (sort(keys %ENV)) {
#     print "$k $ENV{$k}<br>\n";
# }

if (defined $ENV{"REQUEST_METHOD"}) {
    if ($ENV{"REQUEST_METHOD"} eq "GET") {
        if (defined $ENV{"QUERY_STRING"}) {
            $::buffer = $ENV{"QUERY_STRING"};
        } else {
            $::buffer = "";
        }
        ProcessFormFields $::buffer;
    } else {
        if (exists($ENV{"CONTENT_TYPE"}) && $ENV{"CONTENT_TYPE"} =~
            m@multipart/form-data; boundary=\s*([^; ]+)@) {
            ProcessMultipartFormFields($1);
            $::buffer = "";
        } else {
            read STDIN, $::buffer, $ENV{"CONTENT_LENGTH"} ||
                die "Couldn't get form data";
            ProcessFormFields $::buffer;
        }
    }
}

if (defined $ENV{"HTTP_COOKIE"}) {
    # Don't trust anything which came in as a cookie
    use re 'taint';
    foreach my $pair (split(/;/, $ENV{"HTTP_COOKIE"})) {
        $pair = trim($pair);
        if ($pair =~ /^([^=]*)=(.*)$/) {
            $::COOKIE{$1} = $2;
        } else {
            $::COOKIE{$pair} = "";
        }
    }
}

1;
