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

# Takes two files, and creates a file with changebars.

set file1 [lvarpop argv]
set file2 [lvarpop argv]

set data(foo) {}
unset data(foo)

set count 0
for_file line $file1 {
    incr count
    set data($count) $line
    set mark($count) " "
}

set fid [open "|/usr/local/bin/diff -b -e $file1 $file2" "r"]


proc Delete {first last} {
    global data
    for {set i $first} {$i<=$last} {incr i} {
        unset data($i)
    }
}

proc Add {first fid} {
    global data mark
    set sub 10000
    while {[gets $fid line] >= 0} {
        if {[cequal $line "."]} {
            break
        }
        set data($first.$sub) $line
        set mark($first.$sub) "|"
        incr sub
    }
}



while {[gets $fid line] >= 0} {
    if {![regexp {^(.*),(.*)(a|c|d)$} $line foo first last]} {
        set first [crange $line 0 end-1]
        set last $first
    }
    switch [cindex $line end] {
        d {
            Delete $first $last
        }
        c {
            Delete $first $last
            Add $first $fid
        }
        a {
            Add $first $fid
        }
    }
}

catch {close $fid}

foreach i [lsort -real [array names data]] {
    puts "$mark($i) $data($i)"
}
