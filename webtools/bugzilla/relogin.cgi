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

use diagnostics;
use strict;

require "CGI.pl";


print "Set-Cookie: Bugzilla_login= ; path=/; expires=Sun, 30-Jun-80 00:00:00 GMT
Set-Cookie: Bugzilla_logincookie= ; path=/; expires=Sun, 30-Jun-80 00:00:00 GMT
Set-Cookie: Bugzilla_password= ; path=/; expires=Sun, 30-Jun-80 00:00:00 GMT
Content-type: text/html

";
PutHeader("Your login has been forgotten");
print "
The cookie that was remembering your login is now gone.  The next time you
do an action that requires a login, you will be prompted for it.
<p>
<A HREF=\"query.cgi\">Back to the query page.</A>
";

exit;

# The below was a different way, that prompted you for a login right then.

# catch {unset COOKIE(Bugzilla_login)}
# catch {unset COOKIE(Bugzilla_password)}
# confirm_login

# puts "Content-type: text/html\n"
# puts "<H1>OK, logged in.</H1>"
# puts "You are now logged in as <b>$COOKIE(Bugzilla_login)</b>."
# puts "<p>"
# puts "<a href=query.cgi>Back to the query page.</a>"

