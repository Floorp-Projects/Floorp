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

source globals.tcl

set treeid [lindex $argv 0]
LoadTreeConfig
set modulename $treeinfo($treeid,module)

Log "Attempting to recreate legaldirs..."

proc digest {str} {
    global array
    set key [lvarpop str]
    if {[cequal [cindex [lindex $str 0] 0] "-"]} {
        lvarpop str
    }
    set array($key) $str
}



set env(CVSROOT) $treeinfo($treeid,repository)
set origdir [pwd]
cd /
set fid [open "|/tools/ns/bin/cvs checkout -c" r]
cd $origdir

set curline ""
while {[gets $fid line] >= 0} {
    if {[ctype space [cindex $line 0]]} {
        append curline $line
    } else {
        digest $curline
        set curline $line
    }
}
digest $curline
close $fid

if {![info exists array($modulename)]} {
    error "modules file no longer includes $modulename ???"
}
    
set oldlist {}
set list $modulename

while {![cequal $list $oldlist]} {
    set oldlist $list
    set list {}
    foreach i $oldlist {
        if {[info exists array($i)]} {
            set list [concat $list $array($i)]
            # Do an unset to prevent infinite recursion.
            unset array($i)
        } else {
            lappend list $i
        }
    }
}





Lock
set fid [open "[DataDir]/legaldirs" "w"]
catch {chmod 0666 "[DataDir]/legaldirs"}
foreach i $list {
    puts $fid $i
    puts $fid "$i/*"
}
close $fid
Log "...legaldirs recreated."
Unlock

exit
