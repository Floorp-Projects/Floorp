#! /usr/bonsaitools/bin/mysqltcl
# -*- Mode: tcl; indent-tabs-mode: nil -*-
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
source "CGI.tcl"

confirm_login

if {![info exists FORM(pwd1)]} {
    puts "Content-type: text/html

<H1>Change your password</H1>
<form method=post>
<table>
<tr>
<td align=right>Please enter the new password for <b>$COOKIE(Bugzilla_login)</b>:</td>
<td><input type=password name=pwd1></td>
</tr>
<tr>
<td align=right>Re-enter your new password:</td>
<td><input type=password name=pwd2></td>
</table>
<input type=submit value=Submit>"
    exit
}

if {![cequal $FORM(pwd1) $FORM(pwd2)]} {
    puts "Content-type: text/html

<H1>Try again.</H1>
The two passwords you entered did not match.  Please click <b>Back</b> and try again."
    exit
}


set pwd $FORM(pwd1)


if {![regexp {^[a-zA-Z0-9-_]*$} $pwd] || [clength $pwd] < 3 || [clength $pwd] > 15} {
    puts "Content-type: text/html

<H1>Sorry; we're picky.</H1>
Please choose a password that is between 3 and 15 characters long, and that
contains only numbers, letters, hyphens, or underlines.
<p>
Please click <b>Back</b> and try again."
    exit
}


puts "Content-type: text/html\n"

SendSQL "update profiles set password='$pwd' where login_name='[SqlQuote $COOKIE(Bugzilla_login)]'"

puts "<H1>OK, done.</H1>
Your new password has been set.
<p>
<a href=query.cgi>Back to query page.</a>"
