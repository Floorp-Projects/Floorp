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
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 David Gardiner <david.gardiner@unisa.edu.au>
#                 Joe Robins <jmrobins@tgix.com>
#                 Christopher Aillon <christopher@aillon.com>

use diagnostics;
use strict;

require "CGI.pl";

# Shut up misguided -w warnings about "used only once":

use vars %::FORM;

ConnectToDatabase();

# Clear out the login cookies.  Make people log in again if they create an
# account; otherwise, they'll probably get confused.

print "Set-Cookie: Bugzilla_login= ; path=/; expires=Sun, 30-Jun-80 00:00:00 GMT
Set-Cookie: Bugzilla_logincookie= ; path=/; expires=Sun, 30-Jun-80 00:00:00 GMT
Set-Cookie: Bugzilla_password= ; path=/; expires=Sun, 30-Jun-80 00:00:00 GMT
Content-type: text/html

";

# If we're using LDAP for login, then we can't create a new account here.
if(Param('useLDAP')) {
  PutHeader("Can't create LDAP accounts");
  print "This site is using LDAP for authentication.  Please contact an LDAP ";
  print "administrator to get a new account created.\n";
  PutFooter();
  exit;
}

my $login = $::FORM{'login'};
my $realname = trim($::FORM{'realname'});
if (defined $login) {
    CheckEmailSyntax($login);
    if (DBname_to_id($login) != 0) {
        PutHeader("Account Exists");
        print qq|
          <form method="get" action="token.cgi">
            <input type="hidden" name="a" value="reqpw">
            <input type="hidden" name="loginname" value="$login">
            A Bugzilla account for <tt>$login</tt> already exists.  If you
            are the account holder and have forgotten your password, 
            <input type="submit" value="submit a request to change it">.
          </form>
        |;
        PutFooter();
        exit;
    }
    PutHeader("Account created");
    my $password = InsertNewUser($login, $realname);
    MailPassword($login, $password);
    print " You can also <a href=query.cgi?GoAheadAndLogIn>click\n";
    print "here</a> to log in for the first time.";
    PutFooter();
    exit;
}

PutHeader("Create a new bugzilla account");
print q{
To create a bugzilla account, all that you need to do is to enter a
legitimate e-mail address.  The account will be created, and its
password will be mailed to you. Optionally you may enter your real name 
as well.

<FORM method=get>
<table>
<tr>
<td align=right><b>E-mail address:</b></td>
<td><input size=35 name=login></td>
</tr>
<tr>
<td align=right><b>Real name:</b></td>
<td><input size=35 name=realname></td>
</tr>
</table>
<input type="submit" value="Create Account">
};

PutFooter();
