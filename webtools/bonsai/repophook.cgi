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

set startfrom [ParseTimeAndCheck [FormData startfrom]]

Lock
LoadTreeConfig
LoadCheckins
set checkinlist {}
WriteCheckins
Unlock


puts "<TITLE> Rebooting, please wait...</TITLE>

<H1>Recreating the hook</H1>

<h3>$treeinfo($treeid,description)</h3>

<p>
Searching for first checkin after [MyFmtClock $startfrom]...<p>"

flush stdout

regsub -all -- / $treeinfo($treeid,repository) _ mungedname
set filename "data/checkinlog$mungedname"

set fid [open $filename "r"]


set foundfirst 0

set buffer {}


set tempfile data/repophook.[id process]

proc FlushBuffer {} {
    global buffer tempfile treeid foundfirst count
    if {!$foundfirst || [cequal $buffer ""]} {
        return
    }
    write_file $tempfile "junkline\n\n$buffer"
    exec ./addcheckin.tcl -treeid $treeid $tempfile
    unlink $tempfile
    set buffer {}
    incr count
    if {$count % 100 == 0} {
        puts "$count scrutinized...<br>"
        flush stdout
    }
}

set now [getclock]
set count 0
set lastdate 0

while {[gets $fid line] >= 0} {
    switch -glob -- $line {
        {?|*} {
            lassign [split $line "|"] chtype date
            if {$date < $lastdate} {
                puts "Ick; dates out of order!<br>"
                puts "<pre>[value_quote $line]</pre><p>"
            }
            set $lastdate $date
            if {$foundfirst} {
                append buffer "$line\n"
            } else {
                if {$date >= $startfrom} {
                    if {$date >= $now} {
                        puts "Found a future date! (ignoring):<br>"
                        puts "<pre>[value_quote $line]</pre><p>"
                        flush stdout
                    } else {
                        set foundfirst 1
                        puts "Found first line: <br><pre>[value_quote $line]</pre><p>"
                        puts "OK, now processing checkins...<p>"
                        flush stdout
                        set buffer "$line\n"
                        set count 0
                    }
                } else {
                    incr count
                    if {$count % 2000 == 0} {
                        puts "Skipped $count lines...<p>"
                        flush stdout
                    }
                }
            }
        }
        {:ENDLOGCOMMENT} {
            append buffer "$line\n"
            FlushBuffer
        }
        default {
            append buffer "$line\n"
        }
    }
}

FlushBuffer
        
                
catch {unset checkinlist}
LoadCheckins

puts "Done.  [llength $checkinlist] relevant checkins were found."

PutsTrailer
