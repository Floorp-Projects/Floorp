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

proc AdminOpenTree {lastgood clearp} {
    global lastgoodtimestamp treeopen checkinlist
    if {$treeopen} {
        return
    }
    set lastgoodtimestamp $lastgood
    set treeopen 1
    PickNewBatchID
    if {$clearp} {
        SendMail treeopened
        set checkinlist {}
    } else {
        SendMail treeopenedsamehook
    }
    Log "Tree opened.  lastgood is [MyFmtClock $lastgoodtimestamp]"
}


proc AdminCloseTree {closetime} {
    global closetimestamp treeopen
    if {!$treeopen} {
        return
    }
    set closetimestamp $closetime
    set treeopen 0
    SendMail treeclosed
    Log "Tree closed.  closetime is [MyFmtClock $closetimestamp]"
}

    
proc MakeHookList {} {
    global checkinlist

    # First, the hack to make an empty array.
    set people(zzz) 1
    unset people(zzz)
    
    foreach c $checkinlist {
        upvar #0 $c info
        set people($info(person)) 1
    }

    return [lsort [array names people]]
}
   


proc SendMail {filename} {
    set hooklist [join [MakeHookList] ", "]
    if {[lempty $hooklist]} {
        return
    }

    set fullfilename [DataDir]/$filename
    if {[file exists $fullfilename]} {
        set text [read_file $fullfilename]
    } else {
        set text ""
    }

    foreach k {hooklist} {
        regsub -all -- "%$k%" $text [set $k] text
    }

    exec /usr/lib/sendmail -t << $text
}
