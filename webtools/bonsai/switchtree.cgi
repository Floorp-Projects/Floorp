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

LoadTreeConfig

proc IsChecked {value} {
    global treeid
    if {[cequal $value $treeid]} {
        return "CHECKED"
    } else {
        return ""
    }
}

puts "Content-type: text/html

<HTML>
<TITLE>George, George, George of the jungle...</TITLE>

Which tree would you like to see?
"

puts "<FORM method=get action=\"toplevel.cgi\">"

foreach i $treelist {
    if {![info exists treeinfo($i,nobonsai)]} {
        puts "<INPUT TYPE=radio NAME=treeid VALUE=$i [IsChecked $i]>"
        puts "$treeinfo($i,description)<BR>"
    }
}

puts "<INPUT TYPE=SUBMIT Value=\"Submit\"></FORM>"

PutsTrailer

exit
