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
source adminfuncs.tcl

proc GetDate {line} {
    set date [getclock]
    if [regexp -- {[0-9].*$} $line str] {
        catch {set date [convertclock $str]}
    }
    return $date
}



Lock

for_file line $argv {
    if {[scan $line "%s %s" foobar theTree] != 2} {
        continue
    }
    set treeid $theTree
    switch -glob -- $foobar {
        OPENNOCLEAR -
        opennoclear {
            LoadCheckins
            AdminOpenTree [GetDate $line] 0
            WriteCheckins
        }
        OPEN -
        open {
            LoadCheckins
            AdminOpenTree [GetDate $line] 1
            WriteCheckins
        }
        CLOSE -
        close {
            LoadCheckins
            AdminCloseTree [GetDate $line]
            WriteCheckins
        }
    }
}

Unlock

exit 0
            
            
