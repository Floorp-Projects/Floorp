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

ConnectToDatabase

set repository [lindex $argv 0]
set startdate [lindex $argv 1]

if {![cequal $startdate ""]} {
    set startdate [convertclock $startdate]
}


regsub -all -- / $repository _ mungedname

set infid [open data/checkinlog$mungedname "r"]

# SendSQL "select descid from checkins where repository='[SqlQuote $repository]'"
# 
# set ids(zz) 1
# unset ids(zz)
# while {[MoreSQLData]} {
#     set ids([lindex [FetchSQLData] 0]) 1
# }
# 
# set list [array names ids]
# 
# 
# set num [llength $list]
# 
# set d 0
# foreach i $list {
#     SendSQL "delete from descs where id=$i"
#     incr d
#     if {$d % 100 == 0} {
#       puts "Deleted $d of $num old descriptions <BR>"
#     }
# }
#     
# 
# SendSQL "delete from checkins where repository = '[SqlQuote $repository]'"


set buffer {}
set desc {}
set indesc 0

if {![cequal $startdate ""]} {
    puts "Searching for first entry after [fmtclock $startdate] ... <BR>"
    flush stdout

    if {![catch {set index [open data/index$mungedname r]}]} {
        while {[gets $index line] >= 0} {
            lassign [split $line "|"] offset date
            if {$date <= $startdate} {
                seek $infid $offset
                puts "Seeked to position $offset. <BR>"
                flush stdout
                break
            }
        }
        close $index
    }

    set count 0
    set foundfirst 0
    while {!$foundfirst && [gets $infid line] >= 0} {
        if {$indesc} {
            if {[cequal $line ":ENDLOGCOMMENT"]} {
                set indesc 0
            }
        } else {
            if {[cequal $line "LOGCOMMENT"]} {
                set indesc 1
            } else {
                lassign [split $line "|"] chtype date
                set problem [catch {
                    if {$date >= $startdate} {
                        if {$date - $startdate < 10 * 24 * 60 * 60} {
                            set foundfirst 1
                        }
                    }
                    incr count
                    if {$count % 100 == 0} {
                        puts "[fmtclock $date] <BR>"
                        flush stdout
                    }
                } errorinfo]
                if {$problem} {
                    puts "Date parsing problem? $date  $errorinfo"
                    flush stdout
                }
            }
        }
    }

    set buffer "$line\n"
    puts "OK, found first line:\n$buffer <BR>"
    flush stdout
}

set done 0
while {[gets $infid line] >= 0} {
    if {$indesc} {
        if {[cequal $line ":ENDLOGCOMMENT"]} {
            if {$done % 100 == 0} {
                lassign [split $buffer "|"] chtype date
                if {[catch {set prettydate [fmtclock $date]}]} {
                    puts "Couldn't format date; buffer is $buffer "
                } else {
                    puts "$done done. ([fmtclock $date])<BR>"
                    flush stdout
                }
            }
            AddToDatabase $buffer $desc
            set buffer {}
            set desc {}
            set indesc 0
            incr done
        } else {
            append desc $line
            append desc "\n"
        }
    } else {
        if {[cequal $line "LOGCOMMENT"]} {
            set indesc 1
        } else {
            append buffer $line
            append buffer "\n"  }
    }
}
close $infid

# Now get rid of any dangling descriptions.  Sigh; there ought to be a better 
# way.

puts "Gathering unique ids from descs ... <BR>"
flush stdout
SendSQL "lock tables descs read,checkins read"
SendSQL "select id from descs"
set ids(zz) 1
unset ids(zz)
while {[MoreSQLData]} {
    set ids([lindex [FetchSQLData] 0]) 1
}
puts "... [array size ids] found. <br>"
puts "Gathering unique ids from checkins ... <BR>"
flush stdout
set cids(zz) 1
unset cids(zz)
SendSQL "select distinct descid from checkins"
while {[MoreSQLData]} {
    set cids([lindex [FetchSQLData] 0]) 1
}
puts "... [array size cids] found. <br>"
flush stdout

set n 0
set remove {}
foreach i [array names ids] {
    if {![info exists cids($i)]} {
        lappend remove $i
        incr n
    }
}

puts "Removing $n unused descs: <br>"
SendSQL "unlock tables"

set n 0
foreach i $remove {
    SendSQL "delete from descs where id = $i"
    incr n
    if {$n % 100 == 0} {
        puts "Deleted $n. <br>"
        flush stdout
    }
}
