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
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 David Gardiner <david.gardiner@unisa.edu.au>
#                 Joe Robins <jmrobins@tgix.com>
#                 Christopher Aillon <christopher@aillon.com>
#                 Gervase Markham <gerv@gerv.net>

use diagnostics;
use strict;

use lib qw(.);

require "CGI.pl";

# Shut up misguided -w warnings about "used only once":
use vars qw(
  %FORM
  $template
  $vars
);

ConnectToDatabase();

# If we're using LDAP for login, then we can't create a new account here.
if(Param('useLDAP')) {
  DisplayError("This site is using LDAP for authentication.  Please contact 
                an LDAP administrator to get a new account created.",
               "Can't create LDAP accounts");
  PutFooter();
  exit;
}

# Clear out the login cookies.  Make people log in again if they create an
# account; otherwise, they'll probably get confused.
my $cookiepath = Param("cookiepath");
print "Set-Cookie: Bugzilla_login= ; path=$cookiepath; expires=Sun, 30-Jun-80 00:00:00 GMT
Set-Cookie: Bugzilla_logincookie= ; path=$cookiepath; expires=Sun, 30-Jun-80 00:00:00 GMT\n";

print "Content-Type: text/html\n\n";

my $login = $::FORM{'login'};
my $realname = trim($::FORM{'realname'});

if (defined($login)) {
    # We've been asked to create an account.
    CheckEmailSyntax($login);
    trick_taint($login);
    $vars->{'login'} = $login;
    
    if (!ValidateNewUser($login)) {
        # Account already exists        
        $template->process("admin/account_exists.tmpl", $vars)
          || DisplayError("Template process failed: " . $template->error());
        exit;
    }
    
    # Create account
    my $password = InsertNewUser($login, $realname);
    MailPassword($login, $password);
    
    $template->process("admin/account_created.tmpl", $vars)
      || DisplayError("Template process failed: " . $template->error());
    exit;
}

# Show the standard "would you like to create an account?" form.
$template->process("admin/create_account.tmpl", $vars)
  || DisplayError("Template process failed: " . $template->error())
  && exit;
