#!/usr/bonsaitools/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
# 
# The Original Code is the Bugzilla Bug Tracking System.
# 
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.
# 
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 David Gardiner <david.gardiner@unisa.edu.au>

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

PutHeader("Create a new bugzilla account");

my $login = $::FORM{'login'};
if (defined $login) {
    CheckEmailSyntax($login);
    if (DBname_to_id($login) != 0) {
	print "A bugzilla account for the name <tt>$login</tt> already\n";
	print "exists.  If you have forgotten the password for it, then\n";
	print "<a href=query.cgi?GoAheadAndLogIn>click here</a> and use\n";
	print "the <b>E-mail me a password</b> button.\n";
	exit;
    }
    DBNameToIdAndCheck($login, 1);
    print "A bugzilla account for <tt>$login</tt> has been created.  The\n";
    print "password has been e-mailed to that address.  When it is\n";
    print "received, you may <a href=query.cgi?GoAheadAndLogIn>click\n";
    print "here</a> and log in.  Or, you can just <a href=\"\">go back to\n";
    print "the top</a>.";
    exit;
}

print q{
To create a bugzilla account, all that you need to do is to enter a
legitimate e-mail address.  The account will be created, and its
password will be mailed to you.

<FORM method=get>
<table>
<tr>
<td align=right><b>E-mail address:</b></td>
<td><input size=35 name=login></td>
</tr>
</table>
<input type=submit>
};

