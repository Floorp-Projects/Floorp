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

# Takes something that was quoted for a URL line (%20 for spaces and stuff),
# and returns back the intended string.
proc url_decode {buf} {
    regsub -all {\\(.)} $buf {\1} buf ; regsub -all {\\} $buf {\\\\} buf ;
    regsub -all { }  $buf {\ } buf ; regsub -all {\+} $buf {\ } buf ;
    regsub -all {\$} $buf {\$} buf ; regsub -all \n   $buf {\n} buf ;
    regsub -all {;}  $buf {\;} buf ; regsub -all {\[} $buf {\[} buf ;
    regsub -all \" $buf \\\" buf ; regsub  ^\{ $buf \\\{ buf ;
    regsub -all -nocase {%([a-fA-F0-9][a-fA-F0-9])} $buf {[format %c 0x\1]} buf
    regsub -all "\r\n" $buf "\n" buf
    regsub -all "\r" $buf "\n" buf
    eval return \"$buf\"
}

proc lookup { a key } {
    global $a
    set ref [format %s(%s) $a $key]
    if { [ info exists $ref] } {
        eval return \$$ref
    } else {
        return ""
    }
}

proc FormData { field } {
  global FORM
  return [join $FORM($field)]
}

proc ProcessFormFields {buffer} {
    global FORM treeid
    catch {unset FORM}
    foreach item [ split $buffer & ] {
        set el [ split $item = ]
        set value [url_decode [lindex $el 1]]
        set name [lindex $el 0]
        if { $value != "" } {
            lappend FORM($name) $value
        } else {
            set isnull($name) 1
        }
    }
    if {[info exists isnull]} {
        foreach name [array names isnull] {
            if {![info exists FORM($name)]} {
                set FORM($name) ""
            }
        }
    }
    if {[info exists FORM(treeid)]} {
        set treeid $FORM(treeid)
    }
    if {[info exists FORM(batchid)]} {
        global batchid readonly
        source [DataDir]/batchid
        if {$batchid != $FORM(batchid)} {
            set batchid $FORM(batchid)
            set readonly 1
        }
    }
}


proc BatchIdPart {{initstr ""}} {
    global treeid batchid readonly
    set result ""
    if {![cequal $treeid "default"] || $readonly} {
        set result $initstr
    }
    if {![cequal $treeid "default"]} {
        append result "&treeid=$treeid"
    }
    if {$readonly} {
        append result "&batchid=$batchid"
    } 
    return $result
}


if { [info exists env(REQUEST_METHOD) ] } {
    if { $env(REQUEST_METHOD) == "GET" } { 
        set buffer [lookup env QUERY_STRING]
    } else { set buffer [ read stdin $env(CONTENT_LENGTH) ] }
    ProcessFormFields $buffer
}

proc value_quote {var} {
    regsub -all {&} "$var" {\&amp;} var
    regsub -all {"} "$var" {\&quot;} var
    regsub -all {<} "$var" {\&lt;} var
    regsub -all {>} "$var" {\&gt;} var
    return $var
}

foreach pair [ split [lookup env HTTP_COOKIE] ";" ] {
    set pair [string trim $pair]
    set eq [string first = $pair ]
    if {$eq == -1} {
        set COOKIE($pair) ""
    } else {
        set COOKIE([string range $pair 0 [expr $eq - 1]]) [string range $pair [expr $eq + 1] end]
    }
}


proc PutsTrailer {} {
    puts "<hr><a href=\"toplevel.cgi[BatchIdPart ?]\" target=_top>Back to the top of Bonsai</a>"
}
