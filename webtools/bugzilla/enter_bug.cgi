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


source CGI.tcl

if {![info exists FORM(product)]} {
    GetVersionTable
    if {[array size versions] != 1} {
        puts "Content-type: text/html\n"
        PutHeader "Enter Bug" "Enter Bug"
        
        puts "<H2>First, you must pick a product on which to enter a bug.</H2>"
        foreach p [lsort [array names versions]] {
            puts "<a href=\"enter_bug.cgi?product=[url_quote $p]\"&$buffer>$p</a><br>"
        }
        exit
    }
    set FORM(product) [array names versions]
}

set product $FORM(product)

confirm_login

puts "Content-type: text/html\n"



proc pickplatform {} {
    global env FORM
    if {[formvalue rep_platform] != ""} {
        return [formvalue rep_platform]
    }
    switch -regexp $env(HTTP_USER_AGENT) {
        {Mozilla.*\(X11} {return "X-Windows"}
        {Mozilla.*\(Windows} {return "PC"}
        {Mozilla.*\(Macintosh} {return "Macintosh"}
        {Mozilla.*\(Win} {return "PC"}
        default {return "PC"}
    }
}



proc pickversion {} {
    global env versions product FORM
    
    set version [formvalue version]
    if {$version == ""} {
        regexp {Mozilla[ /]([^ ]*) } $env(HTTP_USER_AGENT) foo version
 
        switch -regexp $env(HTTP_USER_AGENT) {
            {4\.09} { set version "4.5" }
        }
    }
    
    if {[lsearch -exact $versions($product) $version] >= 0} {
        return $version
    } else {
        if {[info exists COOKIE(VERSION-$product)]} {
            if {[lsearch -exact $versions($product) $COOKIE(VERSION-$Product)] >= 0} {
                return $COOKIE(VERSION-$Product)
            }
        }
    }
    return [lindex $versions($product) 0]
}


proc pickcomponent {} {
    global components product FORM
    set result [formvalue component]
    if {![cequal $result ""] && \
            [lsearch -exact $components($product) $result] < 0} {
        set result ""
    }
    return $result
}


proc pickos {} {
    global env FORM
    if {[formvalue op_sys] != ""} {
        return [formvalue op_sys]
    }
    switch -regexp $env(HTTP_USER_AGENT) {
        {Mozilla.*\(.*;.*; IRIX.*\)}    {return "IRIX"}
        {Mozilla.*\(.*;.*; 32bit.*\)}   {return "Windows 95"}
        {Mozilla.*\(.*;.*; 16bit.*\)}   {return "Windows 3.1"}
        {Mozilla.*\(.*;.*; 68K.*\)}     {return "System 7.5"}
        {Mozilla.*\(.*;.*; PPC.*\)}     {return "System 7.5"}
        {Mozilla.*\(.*;.*; OSF.*\)}     {return "OSF/1"}
        {Mozilla.*\(.*;.*; Linux.*\)}   {return "Linux"}
        {Mozilla.*\(.*;.*; SunOS 5.*\)} {return "Solaris"}
        {Mozilla.*\(.*;.*; SunOS.*\)}   {return "SunOS"}
        {Mozilla.*\(.*;.*; SunOS.*\)}   {return "SunOS"}
        {Mozilla.*\(Win16.*\)}          {return "Windows 3.1"}
        {Mozilla.*\(Win95.*\)}          {return "Windows 95"}
        {Mozilla.*\(WinNT.*\)}          {return "Windows NT"}
        default {return "other"}
    }
}

proc formvalue {name {default ""}} {
    global FORM
    if {[info exists FORM($name)]} {
        return [FormData $name]
    }
    return $default
}

GetVersionTable

set assign_element [GeneratePersonInput assigned_to 1 [formvalue assigned_to]]
set cc_element [GeneratePeopleInput cc [formvalue cc ""]]


set priority_popup [make_popup priority $legal_priority [formvalue priority "P2"] 0]
set sev_popup [make_popup bug_severity $legal_severity [formvalue bug_severity "normal"] 0]
set platform_popup [make_popup rep_platform $legal_platform [pickplatform] 0]
set opsys_popup [make_popup op_sys $legal_opsys [pickos] 0]

set component_popup [make_popup component $components($product) \
                         [formvalue component] 1]

PutHeader "Enter Bug" "Enter Bug"

puts "
<FORM NAME=enterForm METHOD=POST ACTION=\"post_bug.cgi\">
<INPUT TYPE=HIDDEN NAME=bug_status VALUE=NEW>
<INPUT TYPE=HIDDEN NAME=reporter VALUE=$COOKIE(Bugzilla_login)>
<INPUT TYPE=HIDDEN NAME=product VALUE=$product>
  <TABLE CELLSPACING=2 CELLPADDING=0 BORDER=0>
  <TR>
    <td ALIGN=right valign=top><B>Product:</B></td>
    <td valign=top>$product</td>
  </TR>
  <TR>
    <td ALIGN=right valign=top><B>Version:</B></td>
    <td>[Version_element [pickversion] $product]</td>
    <td align=right valign=top><b>Component:</b></td>
    <td>$component_popup</td>
  </TR>
  <tr><td>&nbsp<td> <td> <td> <td> <td> </tr>
  <TR>
    <td align=right><b><B><A HREF=\"bug_status.html#rep_platform\">Platform:</A></B></td>
    <TD>$platform_popup</TD>
    <TD ALIGN=RIGHT><B>OS:</B></TD>
    <TD>$opsys_popup</TD>
    <td align=right valign=top></td>
    <td rowspan=3></td>
    <td></td>
  </TR>
  <TR>
    <TD ALIGN=RIGHT><B><A HREF=\"bug_status.html#priority\">Priority</A>:</B></TD>
    <TD>$priority_popup</TD>
    <TD ALIGN=RIGHT><B><A HREF=\"bug_status.html#severity\">Severity</A>:</B></TD>
    <TD>$sev_popup</TD>
    <td></td>
    <td></td>
  </TR>
  <tr><td>&nbsp<td> <td> <td> <td> <td> </tr>
  <tr>
    <TD ALIGN=RIGHT><B><A HREF=\"bug_status.html#assigned_to\">Assigned To:
        </A></B></TD>
    <TD colspan=5>$assign_element
    (Leave blank to assign to default owner for component)</td>
  </tr>
  <tr>
    <TD ALIGN=RIGHT ><B>Cc:</B></TD>
    <TD colspan=5>$cc_element</TD>
  </tr>
  <tr><td>&nbsp<td> <td> <td> <td> <td> </tr>
  <TR>
    <TD ALIGN=RIGHT><B>URL:</B>
    <TD COLSPAN=5>
      <INPUT NAME=bug_file_loc SIZE=60 value=\"[value_quote [formvalue bug_file_loc]]\"></TD>
  </TR>
  <TR>
    <TD ALIGN=RIGHT><B>Summary:</B>
    <TD COLSPAN=5>
      <INPUT NAME=short_desc SIZE=60 value=\"[value_quote [formvalue short_desc]]\"></TD>
  </TR>
  <tr><td>&nbsp<td> <td> <td> <td> <td> </tr>
  <tr>
    <td aligh=right valign=top><B>Description:</b>
    <td colspan=5><TEXTAREA WRAP=HARD NAME=comment ROWS=10 COLS=80>[value_quote [formvalue comment]]</TEXTAREA><BR></td>
  </tr>
  <tr>
    <td></td>
    <td colspan=5>
       <INPUT TYPE=\"submit\" VALUE=\"    Commit    \">
       &nbsp;&nbsp;&nbsp;&nbsp;
       <INPUT TYPE=\"reset\" VALUE=\"Reset\">
       &nbsp;&nbsp;&nbsp;&nbsp;
       <INPUT TYPE=\"submit\" NAME=maketemplate VALUE=\"Remember values as bookmarkable template\">
    </td>
  </tr>
  </TABLE>
  <INPUT TYPE=hidden name=form_name VALUE=enter_bug>
</FORM>

Some fields initialized from your user-agent, <b>$env(HTTP_USER_AGENT)</b>.
If you think it got it wrong, please tell $maintainer what it should have been.

</BODY></HTML>"

flush stdout
