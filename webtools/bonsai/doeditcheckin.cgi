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

set busted 0

if {![info exists $FORM(id)]} {
    set busted 1
} else {

    upvar #0 $FORM(id) info
    
    if {![info exists info(notes)]} {
        set info(notes) ""
    }
    
    foreach i [lsort [array names info]] {
        if {![cequal [FormData "orig$i"] $info($i)]} {
            set busted 1
            set text "Key $i -- orig is [FormData "orig$i"], new is $info($i)"
            break
        }
    }
}

if {$busted} {
    Unlock
    puts "
<TITLE>Oops!</TITLE>
<H1>Someone else has been here!</H1>

It looks like somebody else has changed or deleted this checkin.
Terry was too lazy to implement anything beyond detecting this
condition.  You'd best go start over -- go back to the list of
checkins, look for this checkin again, and decide if you still want to
make your edits."

    PutsTrailer
    exit
}

proc ParseTimeAndCheck {timestr} {
    if {[catch {set result [convertclock $timestr]}]} {
        puts "
<TITLE>Time trap</TITLE>
<H1>Can't grok the time</H1>
You entered a time of <tt>$timestr</tt>, and I can't understand it.  Please
hit <B>Back</B> and try again."
        exit
    }
    return $result
}

if {[info exists FORM(nukeit)]} {
    Log "A checkin for $info(person) has been nuked."
} else {
    Log "A checkin for $info(person) has been modified."
}

set info(date) [ParseTimeAndCheck [FormData datestring]]
foreach i {person dir files notes treeopen log} {
    set info($i) [FormData $i]
}

if {[info exists FORM(nukeit)]} {
    set w [lsearch -exact $checkinlist $FORM(id)]
    if {$w >= 0} {
        set checkinlist [lreplace $checkinlist $w $w]
    }
}

WriteCheckins

puts "OK, the checkin has been changed."

PutsTrailer
exit
