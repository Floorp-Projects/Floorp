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

puts "Content-type: text/html\n"
ConnectToDatabase
if {[info exists FORM(commit)]} {
  puts "<TITLE>Changing Long Description for bug [FormData id]</TITLE>"
  set tmpfile [TmpName ldesc.[pid]]
  SendSQL "select rowid from bugs where bug_id = [FormData id]"
  set r [FetchSQLData]
  set lfile [open $tmpfile w]
  puts -nonewline $lfile [FormData long_desc]
  close $lfile
  orawritelong $oc [lindex $r 0] bugs long_desc $tmpfile
  unlink $tmpfile
  oracommit $oc
  puts "<H1>Long Description Changed</H1>"
  puts "<UL><LI>"
  puts "<A HREF=\"show_bug.cgi?id=[FormData id]\">Show Bug #[FormData id]</A>"
  puts "<LI><A HREF=\"query.cgi\">Query Page</A>"
  exec ./processmail [FormData id] < /dev/null > /dev/null 2> /dev/null &
  exit 0
}


puts "<TITLE>Editing Long Description for bug [FormData id]</TITLE>"

puts "<H2>Be Careful</H2>"
puts "Many people think that the ability to edit the full long description
is a bad thing and that developers should not be allowed to do this,
because information can get lost.  Please do not use this feature casually.
<P><HR>"

set generic_query {
select
  bugs.bug_id,
  bugs.product,
  bugs.version,
  bugs.rep_platform,
  bugs.op_sys,
  bugs.bug_status,
  bugs.bug_severity,
  bugs.priority,
  bugs.resolution,
  assign.login_name,
  report.login_name,
  bugs.bug_file_loc,
  bugs.short_desc
from bugs,profiles assign,profiles report,priorities
where assign.userid = bugs.assigned_to and report.userid = bugs.reporter and
}


SendSQL "$generic_query bugs.bug_id = [FormData id]"
set result [ FetchSQLData ]
puts "<TABLE WIDTH=100%>"
puts "<TD COLSPAN=4><TR><DIV ALIGN=CENTER><B><FONT =\"+3\">[html_quote [lindex $result 13]]</B></FONT></DIV>"
puts "<TR><TD><B>Bug#:</B> [lindex $result 0]"
puts "<TD><B>Product:</B> [lindex $result 1]"
puts "<TD><B>Version:</B> [lindex $result 2]"
puts "<TD><B>Platform:</B> [lindex $result 3]"
puts "<TR><TD><B>OS/Version:</B> [lindex $result 4]"
puts "<TD><B>Status:</B> [lindex $result 5]"
puts "<TD><B>Severity:</B> [lindex $result 6]"
puts "<TD><B>Priority:</B> [lindex $result 7]"
puts "</TD><TD><B>Resolution:</B> [lindex $result 8]</TD>"
puts "<TD><B>Assigned To:</B> [lindex $result 9]"
puts "<TD><B>Reported By:</B> [lindex $result 10]"
puts "<TR><TD COLSPAN=6><B>URL:</B> [html_quote [lindex $result 11]]"
puts "<TR><TD><B>Description:</B>\n</TABLE>"

set ldesc [GetLongDescription [FormData id]]
set lines [llength [split $ldesc "\n"]]
incr lines 10
if {$lines > 100} {
  set lines 100
}
if {[regexp {Macintosh} $env(HTTP_USER_AGENT)]} {
  set cols 160
} else {
  set cols 80
}

puts "<FORM METHOD=POST ACTION=\"edit_desc.cgi\">
<INPUT TYPE=HIDDEN NAME=\"id\" VALUE=$FORM(id)>
<INPUT TYPE=HIDDEN NAME=\"commit\" VALUE=yes>
<TEXTAREA NAME=long_desc WRAP=HARD COLS=$cols ROWS=$lines>
[html_quote $ldesc]
</TEXTAREA>
<INPUT TYPE=SUBMIT VALUE=\"Change Description\">
</FORM>"
