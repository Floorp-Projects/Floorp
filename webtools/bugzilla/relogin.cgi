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

use diagnostics;
use strict;

use vars %::COOKIE;

require "CGI.pl";

my $cookiepath = Param("cookiepath");
print "Set-Cookie: Bugzilla_login= ; path=$cookiepath; expires=Sun, 30-Jun-80 00:00:00 GMT
Set-Cookie: Bugzilla_logincookie= ; path=$cookiepath; expires=Sun, 30-Jun-80 00:00:00 GMT
Content-type: text/html

";
PutHeader ("Relogin");

print "<B>Your login has been forgotten</B>.</P>
The cookie that was remembering your login is now gone.  The next time you
do an action that requires a login, you will be prompted for it.
<p>
";

delete $::COOKIE{"Bugzilla_login"};

PutFooter();

exit;

