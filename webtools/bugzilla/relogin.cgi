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
#                 Gervase Markham <gerv@gerv.net>

use diagnostics;
use strict;

use vars %::COOKIE;
use vars qw($template $vars);

use lib qw(.);

require "CGI.pl";

# We don't want to remove a random logincookie from the db, so
# call quietly_check_login. If we're logged in after this, then
# the logincookie must be correct

ConnectToDatabase();
quietly_check_login();

if ($::userid) {
    # Even though we know the userid must match, we still check it in the
    # SQL as a sanity check, since there is no locking here, and if
    # the user logged out from two machines simulataniously, while someone
    # else logged in and got the same cookie, we could be logging the
    # other user out here. Yes, this is very very very unlikely, but why
    # take chances? - bbaetz
    SendSQL("DELETE FROM logincookies WHERE cookie = " .
            SqlQuote($::COOKIE{"Bugzilla_logincookie"}) .
            "AND userid = $::userid");
}

my $cookiepath = Param("cookiepath");
print "Set-Cookie: Bugzilla_login= ; path=$cookiepath; expires=Sun, 30-Jun-80 00:00:00 GMT
Set-Cookie: Bugzilla_logincookie= ; path=$cookiepath; expires=Sun, 30-Jun-80 00:00:00 GMT
";

# delete the cookie before dumping the header so that it shows the user
# as logged out if %commandmenu% is in the header
delete $::COOKIE{"Bugzilla_login"};

    $vars->{'title'} = "Logged Out";
    $vars->{'message'} = "<b>Your login has been forgotten</b>.
                          The cookie that was remembering your login is 
                          now gone. You will be prompted for a login the 
                          next time it is required.";
    $vars->{'url'} = "query.cgi?GoAheadAndLogIn=1";
    $vars->{'link'} = "Log in again here";
    
    print "Content-Type: text/html\n\n";
    $template->process("global/message.html.tmpl", $vars)
      || ThrowTemplateError($template->error());

exit;

