#!/usr/bonsaitools/bin/mysqltcl
# -*- Mode: tcl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is the Bonsai CVS tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.

source CGI.tcl

LoadCheckins

upvar #0 $FORM(id) info

puts "Content-type: text/html

<HTML>
<TITLE>Say the magic word.</TITLE>
<H1>Edit a checkin.</H1>

Congratulations, you have found the hidden edit-a-checkin feature.  Of course,
you need to know the magic word to do anything from here.

<P>

<FORM method=get action=\"doeditcheckin.cgi\">
<TABLE>
<tr>
<td align=right><B>Password:</B></td>
<td><INPUT NAME=password TYPE=password></td>
</tr><tr>
<td align=right><B>When:</B></td>
<td><INPUT NAME=datestring VALUE=\"[value_quote [MyFmtClock $info(date)]]\">
</td></tr>
"

if {![info exists info(notes)]} {
    set info(notes) ""
}

foreach i {person dir files notes} {
    puts "<tr><td align=right><B>$i:</B></td>"
    puts "<td><INPUT NAME=$i VALUE=\"[value_quote $info($i)]\"></td></tr>"
}

proc CheckString {value} {
    if {$value} {
        return "CHECKED"
    } else {
        return ""
    }
}

puts "
<tr><td align=right><b>Tree state:</b></td>
<td><INPUT TYPE=radio NAME=treeopen VALUE=1 [CheckString $info(treeopen)]>Open
</td></tr><tr><td></td>
<td><INPUT TYPE=radio NAME=treeopen VALUE=0 [CheckString [expr !$info(treeopen)]]>Closed
</td></tr><tr>
<td align=right valign=top><B>Log message:</B></td>
<td><TEXTAREA NAME=log ROWS=10 COLS=80>$info(log)</TEXTAREA></td></tr>
</table>
<INPUT TYPE=CHECKBOX NAME=nukeit>Check this box to blow away this checkin entirely.<br>

<INPUT TYPE=SUBMIT VALUE=Submit>"

foreach i [lsort [array names info]] {
    puts "<INPUT TYPE=HIDDEN NAME=orig$i VALUE=\"[value_quote $info($i)]\">"
}
puts "<INPUT TYPE=HIDDEN NAME=id VALUE=\"[value_quote $FORM(id)]\">"

puts "<INPUT TYPE=HIDDEN NAME=treeid VALUE=\"[value_quote $treeid]\">"



puts "</TABLE></FORM>"

PutsTrailer

exit
