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
puts "Content-type: text/html\n"
puts "<TITLE>Full Text Bug Listing</TITLE>"

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
  bugs.component,
  bugs.bug_file_loc,
  bugs.short_desc
from bugs,profiles assign,profiles report
where assign.userid = bugs.assigned_to and report.userid = bugs.reporter and
}

ConnectToDatabase

foreach bug [split $FORM(buglist) :] {
  SendSQL "$generic_query bugs.bug_id = $bug\n"

  if { [ MoreSQLData ] } {
    set result [ FetchSQLData ]
    puts "<IMG SRC=\"1x1.gif\" WIDTH=1 HEIGHT=80 ALIGN=LEFT>"
    puts "<TABLE WIDTH=100%>"
    puts "<TD COLSPAN=4><TR><DIV ALIGN=CENTER><B><FONT =\"+3\">[html_quote [lindex $result 15]]</B></FONT></DIV>"
    puts "<TR><TD><B>Bug#:</B> <A HREF=\"show_bug.cgi?id=[lindex $result 0]\">[lindex $result 0]</A>"
    puts "<TD><B>Product:</B> [lindex $result 1]"
    puts "<TD><B>Version:</B> [lindex $result 2]"
    puts "<TD><B>Platform:</B> [lindex $result 3]"
    puts "<TR><TD><B>OS/Version:</B> [lindex $result 4]"
    puts "<TD><B>Status:</B> [lindex $result 5]"
    puts "<TD><B>Severity:</B> [lindex $result 6]"
    puts "<TD><B>Priority:</B> [lindex $result 7]"
    puts "<TR><TD><B>Resolution:</B> [lindex $result 8]</TD>"
    puts "<TD><B>Assigned To:</B> [lindex $result 9]"
    puts "<TD><B>Reported By:</B> [lindex $result 10]"
    puts "<TR><TD><B>Component:</B> [lindex $result 11]"
    puts "<TR><TD COLSPAN=6><B>URL:</B> [html_quote [lindex $result 12]]"
    puts "<TR><TD COLSPAN=6><B>Summary&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;:</B> [html_quote [lindex $result 13]]"
    puts "<TR><TD><B>Description&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;:</B>\n</TABLE>"
    puts "<PRE>[html_quote [GetLongDescription $bug]]</PRE>"
    puts "<HR>"
  }
}
puts "<h6>Mozilla Communications Corporation, Company Confidential, read and eat.</h6>"
