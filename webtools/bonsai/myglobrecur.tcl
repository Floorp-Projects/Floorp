# -*- Mode: tcl; indent-tabs-mode: nil -*-

# The below was taken from the tclX distribution (version 7.4a), and modified
# to quietly continue if it runs into a directory it doesn't have permission
# to enter, and also to skip . directories.



#
# globrecur.tcl --
#
#  Build or process a directory list recursively.
#------------------------------------------------------------------------------
# Copyright 1992-1994 Karl Lehenbauer and Mark Diekhans.
#
# Permission to use, copy, modify, and distribute this software and its
# documentation for any purpose and without fee is hereby granted, provided
# that the above copyright notice appear in all copies.  Karl Lehenbauer and
# Mark Diekhans make no representations about the suitability of this
# software for any purpose.  It is provided "as is" without express or
# implied warranty.
#------------------------------------------------------------------------------
# $Id: myglobrecur.tcl,v 1.1 1998/06/16 21:43:04 terry Exp $
#------------------------------------------------------------------------------
#



proc my_for_recursive_glob {var dirlist globlist cmd {depth 1}} {
    upvar $depth $var myVar
    set recurse {}
    foreach dir $dirlist {
        if ![file isdirectory $dir] {
            error "\"$dir\" is not a directory"
        }
        set code 0
        set result {}
        foreach pattern $globlist {
            if {[catch {set list [glob -nocomplain -- $dir/$pattern]}]} {
                continue
            }
            foreach file $list {
                set myVar $file
                set code [catch {uplevel $depth $cmd} result]
                if {$code != 0 && $code != 4} break
            }
            if {$code != 0 && $code != 4} break
        }
        if {$code != 0 && $code != 4} {
            if {$code == 3} {
                return $result
            }
            if {$code == 1} {
                global errorCode errorInfo
                return -code $code -errorcode $errorCode \
                        -errorinfo $errorInfo $result
            }
            return -code $code $result
        }

        if {[catch {set list [readdir $dir]}]} {
            continue
        }
        foreach file $list {
            set file $dir/$file
            if [file isdirectory $file] {
                set fileTail [file tail $file]
                if {![cequal "." [crange $fileTail 0 0]]} {
                    lappend recurse $file
                }
            }
        }
    }
    if ![lempty $recurse] {
        return [my_for_recursive_glob $var $recurse $globlist $cmd \
                    [expr {$depth + 1}]]
    }
    return {}
}
