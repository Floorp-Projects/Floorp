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

set query "
select
        bug_id,
        product,
        version,
        rep_platform,
        op_sys,
        bug_status,
        resolution,
        priority,
        bug_severity,
        component,
        assigned_to,
        reporter,
        bug_file_loc,
        short_desc,
        date_format(creation_ts,'Y-m-d')
from bugs
where bug_id = $FORM(id)";

SendSQL $query
set ret [FetchSQLData]
if {$ret != ""} {
  set count 0
  foreach field { bug_id product version rep_platform op_sys bug_status 
                  resolution priority bug_severity component
                  assigned_to reporter bug_file_loc short_desc 
                  creation_ts} {
    if { [regexp {^\{(.*)\}$} [lindex $ret $count] junk bug($field)] == 0 } {
      set bug($field) [lindex $ret $count]
    }
    incr count
  }
  set error "none"
} else {
    puts "<TITLE>Bug Splat Error</TITLE>"
    puts "<H1>Query Error</H1>Somehow something went wrong.  Possibly if you"
    puts "mail this page to $maintainer, he will be able to fix things.<HR>"
    puts "Bug $FORM(id) not found<H2>Query Text</H2><PRE>$query<PRE>"
    exit 0
}

set bug(assigned_to) [DBID_to_name $bug(assigned_to)]
set bug(reporter) [DBID_to_name $bug(reporter)]
set bug(long_desc) [GetLongDescription $FORM(id)]


GetVersionTable

#
# These should be read from the database ...
#
set resolution_popup [make_options $legal_resolution_no_dup $bug(resolution)]
set platform_popup [make_options $legal_platform $bug(rep_platform)]
set priority_popup [make_options $legal_priority $bug(priority)]
set sev_popup [make_options $legal_severity $bug(bug_severity)]


set component_popup [make_options $components($bug(product)) $bug(component)]

set cc_element "<INPUT NAME=cc SIZE=30 VALUE=\"[ShowCcList $FORM(id)]\">"


if {$bug(bug_file_loc) != "none" && $bug(bug_file_loc) != "NULL" && $bug(bug_file_loc) != ""} {
  set URL "<B><A HREF=\"$bug(bug_file_loc)\">URL:</A></B>"
} else {
  set URL "<B>URL:</B>"
}

puts "
<HEAD><TITLE>Bug $FORM(id) -- [html_quote $bug(short_desc)]</TITLE></HEAD><BODY>
<FORM NAME=changeform METHOD=POST ACTION=\"process_bug.cgi\">
<INPUT TYPE=HIDDEN NAME=\"id\" VALUE=$FORM(id)>
<INPUT TYPE=HIDDEN NAME=\"was_assigned_to\" VALUE=\"$bug(assigned_to)\">
  <TABLE CELLSPACING=0 CELLPADDING=0 BORDER=0><TR>
    <TD ALIGN=RIGHT><B>Bug#:</B></TD><TD>$bug(bug_id)</TD>
    <TD ALIGN=RIGHT><B><A HREF=\"bug_status.html#rep_platform\">Platform:</A></B></TD>
    <TD><SELECT NAME=rep_platform>$platform_popup</SELECT></TD>
    <TD ALIGN=RIGHT><B>Version:</B></TD>
    <TD><SELECT NAME=version>[make_options $versions($bug(product)) $bug(version)]</SELECT></TD>
  </TR><TR>
    <TD ALIGN=RIGHT><B>Product:</B></TD>
    <TD><SELECT NAME=product>[make_options $legal_product $bug(product)]</SELECT></TD>
    <TD ALIGN=RIGHT><B>OS:</B></TD><TD>$bug(op_sys)</TD>
    <TD ALIGN=RIGHT><B>Reporter:</B></TD><TD>$bug(reporter)</TD>
  </TR><TR>
    <TD ALIGN=RIGHT><B><A HREF=\"bug_status.html\">Status:</A></B></TD>
      <TD>$bug(bug_status)</TD>
    <TD ALIGN=RIGHT><B><A HREF=\"bug_status.html#priority\">Priority:</A></B></TD>
      <TD><SELECT NAME=priority>$priority_popup</SELECT></TD>
    <TD ALIGN=RIGHT><B>Cc:</B></TD>
      <TD> $cc_element </TD>
  </TR><TR>
    <TD ALIGN=RIGHT><B><A HREF=\"bug_status.html\">Resolution:</A></B></TD>
      <TD>$bug(resolution)</TD>
    <TD ALIGN=RIGHT><B><A HREF=\"bug_status.html#severity\">Severity:</A></B></TD>
      <TD><SELECT NAME=bug_severity>$sev_popup</SELECT></TD>
    <TD ALIGN=RIGHT><B>Component:</B></TD>
      <TD><SELECT NAME=component>$component_popup</SELECT></TD>
  </TR><TR>
    <TD ALIGN=RIGHT><B><A HREF=\"bug_status.html#assigned_to\">Assigned&nbsp;To:
        </A></B></TD>
      <TD>$bug(assigned_to)</TD>
  </TR><TR>
    <TD ALIGN=\"RIGHT\">$URL
    <TD COLSPAN=6>
      <INPUT NAME=bug_file_loc VALUE=\"$bug(bug_file_loc)\" SIZE=60></TD>
  </TR><TR>
    <TD ALIGN=\"RIGHT\"><B>Summary:</B>
    <TD COLSPAN=6>
      <INPUT NAME=short_desc VALUE=\"[value_quote $bug(short_desc)]\" SIZE=60></TD>
  </TR>
</TABLE>
<br>
<B>Additional Comments:</B>
<BR>
<TEXTAREA WRAP=HARD NAME=comment ROWS=5 COLS=80></TEXTAREA><BR>
<br>
<INPUT TYPE=radio NAME=knob VALUE=none CHECKED>
        Leave as <b>$bug(bug_status) $bug(resolution)</b><br>"

# knum is which knob number we're generating, in javascript terms.

set knum 1

if {[cequal $bug(bug_status) NEW] || [cequal $bug(bug_status) ASSIGNED] || \
        [cequal $bug(bug_status) REOPENED]} {
    if {![cequal $bug(bug_status) ASSIGNED]} {
        puts "<INPUT TYPE=radio NAME=knob VALUE=accept>
            Accept bug (change status to <b>ASSIGNED</b>)<br>"
        incr knum
    }
    if {[clength $bug(resolution)] > 0} {
        puts "<INPUT TYPE=radio NAME=knob VALUE=clearresolution>"
        puts "Clear the resolution (remove the current resolution of"
        puts "<b>$bug(resolution)</b>)<br>"
        incr knum
    }
    puts "<INPUT TYPE=radio NAME=knob VALUE=resolve>
        Resolve bug, changing <A HREF=\"bug_status.html\">resolution</A> to
        <SELECT NAME=resolution
          ONCHANGE=\"document.changeform.knob\[$knum\].checked=true\">
          $resolution_popup</SELECT><br>"
    incr knum
    puts "<INPUT TYPE=radio NAME=knob VALUE=duplicate>
        Resolve bug, mark it as duplicate of bug # 
        <INPUT NAME=dup_id SIZE=6 ONCHANGE=\"document.changeform.knob\[$knum\].checked=true\"><br>"
    incr knum
    set assign_element "<INPUT NAME=assigned_to SIZE=32 ONCHANGE=\"document.changeform.knob\[$knum\].checked=true\" VALUE=$bug(assigned_to)>"

    puts "<INPUT TYPE=radio NAME=knob VALUE=reassign> 
          <A HREF=\"bug_status.html#assigned_to\">Reassign</A> bug to
          $assign_element
        <br>"
    incr knum
    puts "<INPUT TYPE=radio NAME=knob VALUE=reassignbycomponent>
          Reassign bug to owner of selected component<br>"
    incr knum
} else {
    puts "<INPUT TYPE=radio NAME=knob VALUE=reopen> Reopen bug<br>"
    incr knum
    if {[cequal $bug(bug_status) RESOLVED]} {
        puts "<INPUT TYPE=radio NAME=knob VALUE=verify>
        Mark bug as <b>VERIFIED</b><br>"
        incr knum
    }
    if {![cequal $bug(bug_status) CLOSED]} {
        puts "<INPUT TYPE=radio NAME=knob VALUE=close>
        Mark bug as <b>CLOSED</b><br>"
        incr knum
    }
}
 
puts "
<INPUT TYPE=\"submit\" VALUE=\"Commit\">
<INPUT TYPE=\"reset\" VALUE=\"Reset\">
<INPUT TYPE=hidden name=form_name VALUE=process_bug>
<BR>
<FONT size=\"+1\"><B>
 <A HREF=\"show_activity.cgi?id=$FORM(id)\">View Bug Activity</A>
 <A HREF=\"long_list.cgi?buglist=$FORM(id)\">Format For Printing</A>
</B></FONT><BR>
</FORM>
<table><tr><td align=left><B>Description:</B></td><td width=100%>&nbsp;</td>
<td align=right>Opened:&nbsp;$bug(creation_ts)</td></tr></table>
<HR>
<PRE>
[html_quote $bug(long_desc)]
</PRE>
<HR>"

# To add back option of editing the long description, insert after the above
# long_list.cgi line:
#  <A HREF=\"edit_desc.cgi?id=$FORM(id)\">Edit Long Description</A>



navigation_header

puts "</BODY>"
flush stdout
