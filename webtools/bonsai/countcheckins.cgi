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

set maxsize 400

LoadCheckins

puts "Content-type: text/html

<HTML>
<TITLE>Beancounter central.</TITLE>

<H1>Meaningless checkin statistics</H1>

<TABLE BORDER CELLSPACING=2><TR>
<TH>Tree closed</TH>
<TH>Number<BR>of<BR>people<BR>making<BR>changes</TH>
<TH COLSPAN=2>Number of checkins</TH>
</TR>
"

set list {}


foreach i [glob "[DataDir]/batch-*\[0-9\]"] {
    regexp -- {[0-9]*$} $i n
    lappend list $n
}

set list [lsort -integer -decreasing $list]

set first 1

set biggest 1

foreach i $list {
    source [DataDir]/batch-$i
    set num($i) [llength $checkinlist]
    if {$num($i) > $biggest} {
        set biggest $num($i)
    }
    if {$first} {
        set donetime($i) "Current hook"
        set first 0
    } else {
        set donetime($i) [MyFmtClock $closetimestamp]
    }
    catch {unset people}
    set people(zzz) 1
    unset people(zzz)
    foreach c $checkinlist {
        upvar #0 $c info
        set people($info(person)) 1
    }
    set numpeople($i) [array size people]
}

foreach i $list {
    puts "<TR>"
    puts "<TD>$donetime($i)</TD>"
    puts "<TD ALIGN=RIGHT>$numpeople($i)</TD>"
    puts "<TD ALIGN=RIGHT>$num($i)</TD>"
    puts "<TD><table WIDTH=[expr $num($i) * $maxsize / $biggest] bgcolor=green><tr><td>&nbsp;</td></tr></table></TD>"
    puts "</TR>" 
}
puts "</TABLE>"

PutsTrailer

exit
