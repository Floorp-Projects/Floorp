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

proc IsChecked {value} {
    global batchid
    if {[cequal $value $batchid]} {
        return "CHECKED"
    } else {
        return ""
    }
}

puts "Content-type: text/html

<HTML>
<TITLE>Let's do the time warp again...</TITLE>

Which hook would you like to see?
"

set list {}


foreach i [glob "[DataDir]/batch-*\[0-9\]"] {
    regexp -- {[0-9]*$} $i num
    lappend list $num
}

set list [lsort -integer -decreasing $list]

puts "<FORM method=get action=\"toplevel.cgi\">"
puts "<INPUT TYPE=HIDDEN NAME=treeid VALUE=$treeid>"
puts "<INPUT TYPE=SUBMIT Value=\"Submit\"><BR>"

set value [lvarpop list]

puts "<INPUT TYPE=radio NAME=batchid VALUE=$value [IsChecked $value]>"
puts "The current hook.<BR>"

set count 1
foreach i $list {
    set value [lvarpop list]
    puts "<INPUT TYPE=radio NAME=batchid VALUE=$value [IsChecked $value]>"
    source [DataDir]/batch-$i
    puts "Hook for tree that closed on [MyFmtClock $closetimestamp] <BR>"
}

puts "<INPUT TYPE=SUBMIT Value=\"Submit\">"
puts "</FORM>"

PutsTrailer

exit
