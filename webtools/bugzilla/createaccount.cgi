#!/usr/bin/perl -wT
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

use strict;

use lib qw(.);

require "CGI.pl";

# Shut up misguided -w warnings about "used only once":
use vars qw(
  $template
  $vars
);

# If we're using LDAP for login, then we can't create a new account here.
unless (Bugzilla::Auth->can_edit('new')) {
  # Just in case someone already has an account, let them get the correct
  # footer on the error message
  Bugzilla->login();
  ThrowUserError("auth_cant_create_account");
}

# Clear out the login cookies.  Make people log in again if they create an
# account; otherwise, they'll probably get confused.
Bugzilla->logout();

my $cgi = Bugzilla->cgi;
print $cgi->header();

my $login = $cgi->param('login');

if (defined($login)) {
    # We've been asked to create an account.
    my $realname = trim($cgi->param('realname'));
    CheckEmailSyntax($login);
    $vars->{'login'} = $login;
    
    if (!ValidateNewUser($login)) {
        # Account already exists        
        $template->process("account/exists.html.tmpl", $vars)
          || ThrowTemplateError($template->error());
        exit;
    }

    my $createexp = Param('createemailregexp');
    if (!($createexp)
        || ($login !~ /$createexp/)) {
        ThrowUserError("account_creation_disabled");
        exit;
    }
    
    # Create account
    my $password = InsertNewUser($login, $realname);
    MailPassword($login, $password);
    
    $template->process("account/created.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

# Show the standard "would you like to create an account?" form.
$template->process("account/create.html.tmpl", $vars)
  || ThrowTemplateError($template->error());
