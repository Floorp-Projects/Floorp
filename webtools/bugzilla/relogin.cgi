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
#                 Gervase Markham <gerv@gerv.net>

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

my $cgi = Bugzilla->cgi;

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

$cgi->send_cookie(-name => "Bugzilla_login",
                  -expires => "Tue, 15-Sep-1998 21:49:00 GMT");
$cgi->send_cookie(-name => "Bugzilla_logincookie",
                  -expires => "Tue, 15-Sep-1998 21:49:00 GMT");

delete $::COOKIE{"Bugzilla_login"};

$vars->{'message'} = "logged_out";
$vars->{'user'} = {};

print $cgi->header();
$template->process("global/message.html.tmpl", $vars)
  || ThrowTemplateError($template->error());

exit;

