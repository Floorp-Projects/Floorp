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

puts "Content-type: text/html

<HTML>"

CheckPassword $FORM(password)

Lock
LoadCheckins

if {![info exists FORM(command)]} {
    set FORM(command) nocommand
}


set list {}

foreach i [array names FORM] {
    switch -glob -- $i {
        {checkin-*} {
            if {[lsearch -exact $checkinlist $i] >= 0} {
                lappend list $i
            }
        }
    }
}


set origtree $treeid

switch -exact -- $FORM(command) {
    nuke {
        foreach i $list {
            set w [lsearch -exact $checkinlist $i]
            if {$w >= 0} {
                set checkinlist [lreplace $checkinlist $w $w]
            }
        }
        set what "deleted."
    }
    setopen {
        foreach i $list {
            upvar #0 $i info
            set info(treeopen) 1
        }
        set what "modified to be open."
    }

    setclose {
        foreach i $list {
            upvar #0 $i info
            set info(treeopen) 0
        }
        set what "modified to be closed."
    }
    movetree {
        if {[cequal $treeid $FORM(desttree)]} {
            puts "<H1>Pick a different tree</H1>"
            puts "You attempted to move checkins into the tree that they're"
            puts "already in.  Hit <b>Back</b> and try again."
            PutsTrailer
            exit
        }
        foreach i $list {
            set w [lsearch -exact $checkinlist $i]
            if {$w >= 0} {
                set checkinlist [lreplace $checkinlist $w $w]
            }
        }
        WriteCheckins
        unset checkinlist
        set treeid $FORM(desttree)
        unset batchid
        LoadCheckins
        LoadTreeConfig
        foreach i $list {
            lappend checkinlist $i
        }
        set what "moved to the $treeinfo($treeid,description) tree."
    }
    default {
        puts "<h1>No command selected</h1>"
        puts "You need to select one of the radio command buttons at the"
        puts "bottom.  Hit <b>Back</b> and try again."
        PutsTrailer
        exit
    }
}

WriteCheckins
Unlock

puts "
<H1>OK, done.</H1>
The selected checkins have been $what"

set treeid $origtree

PutsTrailer
exit

