# -*- Mode: tcl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
# 
# The Original Code is the Bugzilla Bug Tracking System.
# 
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.
# 
# Contributor(s): Terry Weissman <terry@mozilla.org>

# Contains some global variables and routines used throughout bugzilla.

set maintainer "<a href=mailto:terry@netscape.com>terry@netscape.com</a>"

if { ! [info exists oradbname] } {
  set oradbname "SCOPPROD"
}

set dontchange "--do_not_change--"
set chooseone "--Choose_one:--"

set tmp_dir ""
proc TmpName { tail } {
  global tmp_dir
  if { $tmp_dir == "" } {
    set tmp_dir "/var/tmp/bugzilla"
    if {! [file isdirectory $tmp_dir]} {
      mkdir $tmp_dir
    }
  }
  return "$tmp_dir/$tail"
}

proc ConnectToDatabase {} {
    global mysqlhandle
    if {![info exists mysqlhandle]} {
        set mysqlhandle [mysqlconnect]
        mysqluse $mysqlhandle "bugs"
    }
}

# Useful for my stand-alone debugging
proc DebugConnect {} {
    global COOKIE
    set COOKIE(Bugzilla_login) terry
    set COOKIE(Bugzilla_password) terry
    ConnectToDatabase
}


proc SendSQL { str } {
# puts $str
    global mysqlhandle errorInfo
    if {[catch {mysqlsel $mysqlhandle $str} errmsg]} {
        puts $str
        error "$errmsg - $str" $errorInfo
    }
    return 0
}

proc MoreSQLData {} {
    global mysqlhandle
    set result [mysqlresult $mysqlhandle "rows?"]
    return [expr ![cequal $result ""] && $result > 0]
}

proc FetchSQLData {} {
    global mysqlhandle
    return [mysqlnext $mysqlhandle]
}

proc Disconnect {} {
    global mysqlhandle
    mysqlclose $mysqlhandle
    unset mysqlhandle
}


set legal_opsys { "Windows 3.1" "Windows 95" "Windows NT" "System 7" "System 7.5"
               "7.1.6" "AIX" "BSDI" "HP-UX" "IRIX" "Linux" "OSF/1" "Solaris" "SunOS"
               "other" }


set default_column_list {severity priority platform owner status resolution summary}

set env(TZ) PST8PDT

proc AppendComment {bugid who comment} {
    regsub -all "\r\n" $comment "\n" comment
    if {[cequal $comment "\n"] || [clength $comment] == 0} {
        return
    }
    SendSQL "select long_desc from bugs where bug_id = $bugid"
    set desc [lindex [FetchSQLData] 0]
    append desc "\n\n------- Additional Comments From $who  [fmtclock [getclock] "%D %H:%M"] -------\n"
    append desc $comment
    SendSQL "update bugs set long_desc='[SqlQuote $desc]' where bug_id=$bugid"
}

proc SortIgnoringCase {a b} {
    return [string compare [string tolower $a] [string tolower $b]]
}


proc make_popup { name src default listtype {onchange {}}} {
    set last ""
    set popup "<SELECT NAME=$name"
    if {$listtype > 0} {
        append popup " SIZE=5"
        if {$listtype == 2} {
            append popup " MULTIPLE"
        }
    }
    if {$onchange != ""} {
        append popup " onchange=$onchange"
    }
    append popup ">"
    append popup [make_options $src $default [expr {$listtype == 2 && $default != ""}]]
    append popup "</SELECT>"
    return $popup
}

proc Product_element { prod {onchange {}} } {
    global versions
    return [make_popup product [lsort [array names versions]] $prod 1 \
                $onchange]
}

proc Component_element { comp prod {onchange {}} } {
    global components
    if {![info exists components($prod)]} {
        set componentlist {}
    } else {
        set componentlist $components($prod)
    }
        
    if {![cequal $comp ""] && [lsearch $componentlist $comp] >= 0} {
        set defcomponent $comp
    } else {
        set defcomponent [lindex $componentlist 0]
    }
    return [make_popup component $componentlist $defcomponent 1 ""]
}

proc Version_element { vers prod {onchange {}} } {
  global versions
  if {![info exists versions($prod)]} {
    set versionlist {}
  } else {
    set versionlist $versions($prod)
  }

  set defversion [lindex $versionlist 0]

  if {[lsearch $versionlist $vers] >= 0} {
    set defversion $vers
  }
  return [make_popup version $versionlist $defversion 1 $onchange]
}

proc GenerateVersionTable {} {
    ConnectToDatabase
    SendSQL "select value, program from versions order by value"
    while { [ MoreSQLData ] } {
        set line [FetchSQLData]
        if {$line != ""} {
            set v [lindex $line 0]
            set p1 [lindex $line 1]
            lappend versions($p1) $v
            set varray($v) 1
            set parray($p1) 1
        }
    }

    SendSQL "select value, program from components"
    while { [ MoreSQLData ] } {
        set line [FetchSQLData]
        if {$line != ""} {
            lassign $line c p
            lappend components($p) $c
            set carray($c) 1
            set parray($p) 1
        }
    }

    LearnAboutColumns bugs cols
    set log_columns $cols(-list-)
    foreach i {bug_id creation_ts delta_ts long_desc} {
        set w [lsearch $log_columns $i]
        if {$w >= 0} {
            set log_columns [lreplace $log_columns $w $w]
        }
    }
    set legal_priority [SplitEnumType $cols(priority,type)]
    set legal_severity [SplitEnumType $cols(bug_severity,type)]
    set legal_platform [SplitEnumType $cols(rep_platform,type)]
    set legal_bug_status [SplitEnumType $cols(bug_status,type)]
    set legal_resolution [SplitEnumType $cols(resolution,type)]
    set legal_resolution_no_dup $legal_resolution
    set w [lsearch $legal_resolution_no_dup "DUPLICATE"]
    if {$w >= 0} {
        set legal_resolution_no_dup [lreplace $legal_resolution_no_dup $w $w]
    }

    set list [lsort -command SortIgnoringCase [array names versions]]
    
    set tmpname "versioncache.[id process]"

    set fid [open $tmpname "w"]
    puts $fid [list set log_columns $log_columns]
    foreach i $list {
        puts $fid [list set versions($i) $versions($i)]
        if {![info exists components($i)]} {
            set components($i) {}
        }
    }

    puts $fid [list set legal_versions [lsort -command SortIgnoringCase \
                                            [array names varray]]]
    foreach i [lsort -command SortIgnoringCase [array names components]] {
        puts $fid [list set components($i) $components($i)]
    }
    puts $fid [list set legal_components [lsort -command SortIgnoringCase \
                                              [array names carray]]]
    puts $fid [list set legal_product $list]
    puts $fid [list set legal_priority $legal_priority]
    puts $fid [list set legal_severity $legal_severity]
    puts $fid [list set legal_platform $legal_platform]
    puts $fid [list set legal_bug_status $legal_bug_status]
    puts $fid [list set legal_resolution $legal_resolution]
    puts $fid [list set legal_resolution_no_dup $legal_resolution_no_dup]
    close $fid
    frename $tmpname "versioncache"
    catch {chmod 0666 "versioncache"}
}

# This proc must be called before using legal_product or the versions array.

proc GetVersionTable {} {
    global versions
    set mtime 0
    catch {set mtime [file mtime versioncache]}
    if {[getclock] - $mtime > 3600} {
        GenerateVersionTable
    }
    uplevel #0 {source versioncache}
    if {![info exists versions]} {
        GenerateVersionTable
        uplevel #0 {source versioncache}
        if {![info exists versions]} {
            error "Can't generate version info; tell terry."
        }
    }
}



proc GeneratePersonInput { field required def_value {extraJavaScript {}} } {
    if {![cequal $extraJavaScript ""]} {
        set $extraJavaScript "onChange=\" $extraJavaScript \""
    }
    return "<INPUT NAME=\"$field\" SIZE=32 $extraJavaScript VALUE=\"$def_value\">"
}

proc GeneratePeopleInput { field def_value } {
  return "<INPUT NAME=\"$field\" SIZE=45 VALUE=\"$def_value\">"
}



set cachedNameArray() ""

proc InsertNewUser {username} {
    random seed
    set pwd ""
    loop i 0 8 {
        append pwd [cindex "abcdefghijklmnopqrstuvwxyz" [random 26]]
    }
    SendSQL "insert into profiles (login_name, password) values ('[SqlQuote $username]', '$pwd')"
    return $pwd
}


proc DBID_to_name { id } {
    global cachedNameArray

    if {![info exists cachedNameArray($id)]} {

        SendSQL "select login_name from profiles where userid = $id"
        set r [FetchSQLData]
        if {$r == ""} { set r "__UNKNOWN__" }
        set cachedNameArray($id) $r
    }
    return $cachedNameArray($id)
}

proc DBname_to_id { name } {
    SendSQL "select userid from profiles where login_name = '[SqlQuote $name]'"
    set r [FetchSQLData]
    if {[cequal $r ""]} {
        return 0
    }
    return $r
}

proc DBNameToIdAndCheck {name {forceok 0}} {
    set result [DBname_to_id $name]
    if {$result > 0} {
        return $result
    }
    if {$forceok} {
        InsertNewUser $name
        set result [DBname_to_id $name]
        if {$result > 0} {
            return $result
        }
        puts "Yikes; couldn't create user $name.  Please report problem to"
        puts "$maintainer."
    } else {
        puts "The name <TT>$name</TT> is not a valid username.  Please hit the"
        puts "<B>Back</B> button and try again."
    }
    exit 0
}

proc GetLongDescription { id } {
    SendSQL "select long_desc from bugs where bug_id = $id"
    return [lindex [FetchSQLData] 0]
}

proc ShowCcList {num} {
    set cclist ""
    set comma ""
    SendSQL "select who from cc where bug_id = $num"
    set ccids ""
    while {[MoreSQLData]} {
        lappend ccids [lindex [FetchSQLData] 0]
    }
    set result ""
    foreach i $ccids {
        lappend result [DBID_to_name $i]
    }

    return [join $result ","]
}

proc make_options_new { src default {isregexp 0} } {
    set last "" ; set popup "" ; set found 0
    foreach item $src {
        if { $item == "-blank-" } { set item "" } {
            if {$isregexp ? [regexp $default $item] : [cequal $default $item]} {
                append popup "<OPTION SELECTED VALUE=\"$item\">$item"
                set found 1
            } else {
                append popup "<OPTION VALUE=\"$item\">$item"
            }
        }
    }
    if {!$found && $default != ""} {
        append popup "<OPTION SELECTED>$default"
    }
    return $popup
}


proc Shell {} {
    ConnectToDatabase
    while (1) {
        puts -nonewline "> "
        if {[gets stdin line] < 0} {
            break
        }
        if {[catch {SendSQL $line} errorinfo]} {
            puts "Error -- $errorinfo"
        } else {
            while {[MoreSQLData]} {
                puts [FetchSQLData]
            }
        }
    }
}




# Fills in the given array with info about the columns.  The array gets
# the following entries:
#   -list-  the list of column names
#   <name>,type  the type for the given name

proc LearnAboutColumns {table arrayname} {
    upvar $arrayname a
    catch (unset a)
    SendSQL "show columns from $table"
    set list {}
    while {[MoreSQLData]} {
        lassign [FetchSQLData] name type
        set a($name,type) $type
        lappend list $name
    }
    set a(-list-) $list
}

# If the above returned a enum type, take that type and parse it into the
# list of values.  Assumes that enums don't ever contain an apostrophe!

proc SplitEnumType {str} {
    set result {}
    if {[regexp {^enum\((.*)\)$} $str junk guts]} {
        append guts ","
        while {[regexp {^'([^']*)',(.*)$} $guts junk first guts]} {
            lappend result $first
        }
    }
    return $result
}


proc SqlQuote {str} {
    regsub -all "'" $str "''" str
    # 
    # This next line is quoting hell.  One level of quoting comes from
    # the TCL interpreter, and another level comes from TCL's regular
    # expression parser.  It really works out to "change every
    # backslash to two backslashes".
    regsub -all "\\\\" $str "\\\\\\\\" str

    return $str
}


proc Param {value} {
    global param
    if {[info exists param($value)]} {
        return $param($value)
    }
    # Um, maybe we haven't sourced in the params at all yet.
    catch {uplevel \#0 source "params"}
    if {[info exists param($value)]} {
        return $param($value)
    }
    # Well, that didn't help.  Maybe it's a new param, and the user
    # hasn't defined anything for it.  Try and load a default value
    # for it.
    uplevel #0 source "defparams.tcl"
    WriteParams
    return $param($value)
}
