#! /usr/bonsaitools/bin/mysqltcl
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

source "CGI.tcl"

confirm_login

puts "Content-type: text/html\n"

GetVersionTable

if {![cequal $FORM(product) $dontchange]} {
    set prod [FormData product]
    set vok [expr [lsearch -exact $versions($prod) \
                       [FormData version]] >= 0]
    set cok [expr [lsearch -exact $components($prod) \
                       [FormData component]] >= 0]
    if {!$vok || !$cok} {
        puts "<H1>Changing product means changing version and component.</H1>"
        puts "You have chosen a new product, and now the version and/or"
        puts "component fields are not correct.  (Or, possibly, the bug did"
        puts "not have a valid component or version field in the first place.)"
        puts "Anyway, please set the version and component now.<p>"
        puts "<form>"
        puts "<table>"
        puts "<tr>"
        puts "<td align=right><b>Product:</b></td>"
        puts "<td>$prod</td>"
        puts "</tr><tr>"
        puts "<td align=right><b>Version:</b></td>"
        puts "<td>[Version_element [FormData version] $prod]</td>"
        puts "</tr><tr>"
        puts "<td align=right><b>Component:</b></td>"
        puts "<td>[Component_element [FormData component] $prod]</td>"
        puts "</tr>"
        puts "</table>"
        foreach i [array names FORM] {
            if {[lsearch -exact {version component} $i] < 0} {
                puts "<input type=hidden name=$i value=\"[value_quote $FORM($i)]\">"
            }
        }
        puts "<input type=submit value=Commit>"
        puts "</form>"
        puts "</hr>"
        puts "<a href=query.cgi>Cancel all this and go back to the query page.</a>"
        exit
    }
}


if {[info exists FORM(id)]} {
    set idlist $FORM(id)
} else {
    set idlist {}
    foreach i [array names FORM] {
        if {[string match "id_*" $i]} {
            lappend idlist [crange $i 3 end]
        }
    }
}

if {![info exists FORM(who)]} {
    set FORM(who) $COOKIE(Bugzilla_login)
}

puts "<TITLE>Update Bug $idlist</TITLE>"
if {[info exists FORM(id)]} {
    navigation_header
}
puts "<HR>"
set query "update bugs\nset"
set comma ""
umask 0

proc DoComma {} {
    global query comma
    append query "$comma\n    "
    set comma ","
}

proc ChangeStatus {str} {
    global dontchange query
    if {![cequal $str $dontchange]} {
        DoComma
        append query "bug_status = '$str'"
    }
}

proc ChangeResolution {str} {
    global dontchange query
    if {![cequal $str $dontchange]} {
        DoComma
        append query "resolution = '$str'"
    }
}




foreach field {rep_platform priority bug_severity url summary \
                   component bug_file_loc short_desc \
                   product version component} {
    if {[info exists FORM($field)]} {
        if {![cequal $FORM($field) $dontchange]} {
            DoComma
            regsub -all "'" [FormData $field] "''" value
            append query "$field = '$value'"
        }
    }
}



ConnectToDatabase

switch -exact $FORM(knob) {
    none {}
    accept {
        ChangeStatus ASSIGNED
    }
    clearresolution {
        ChangeResolution {}
    }
    resolve {
        ChangeStatus RESOLVED
        ChangeResolution $FORM(resolution)
    }
    reassign {
        ChangeStatus NEW
        DoComma
        set newid [DBNameToIdAndCheck $FORM(assigned_to)]
        append query "assigned_to = $newid"
    }
    reassignbycomponent {
        if {[cequal $FORM(component) $dontchange]} {
            puts "You must specify a component whose owner should get assigned"
            puts "these bugs."
            exit 0
        }
        ChangeStatus NEW
        DoComma
        SendSQL "select initialowner from components
where program='[SqlQuote $FORM(product)]'
and value='[SqlQuote $FORM(component)]'"
        set newname [lindex [FetchSQLData] 0]
        set newid [DBNameToIdAndCheck $newname 1]
        append query "assigned_to = $newid"
    }   
    reopen {
        ChangeStatus REOPENED
    }
    verify {
        ChangeStatus VERIFIED
    }
    close {
        ChangeStatus CLOSED
    }
    duplicate {
        ChangeStatus RESOLVED
        ChangeResolution DUPLICATE
        set num $FORM(dup_id)
        if {[catch {incr num}]} {
            puts "You must specify a bug number of which this bug is a"
            puts "duplicate.  The bug has not been changed."
            exit
        }
        if {$FORM(dup_id) == $FORM(id)} {
            puts "Nice try.  But it doesn't really make sense to mark a bug as"
            puts "a duplicate of itself, does it?"
            exit
        }
        AppendComment $FORM(dup_id) $FORM(who) "*** Bug $FORM(id) has been marked as a duplicate of this bug. ***"
        append FORM(comment) "\n\n*** This bug has been marked as a duplicate of $FORM(dup_id) ***"
        exec ./processmail $FORM(dup_id) < /dev/null > /dev/null 2> /dev/null &
    }
    default {
        puts "Unknown action $FORM(knob)!"
        exit
    }
}


if {[lempty $idlist]} {
    puts "You apparently didn't choose any bugs to modify."
    puts "<p>Click <b>Back</b> and try again."
    exit
}

if {[cequal $comma ""]} {
    set comment {}
    if {[info exists FORM(comment)]} {
        set comment $FORM(comment)
    }
    if {[cequal $comment ""]} {
        puts "Um, you apparently did not change anything on the selected bugs."
        puts "<p>Click <b>Back</b> and try again."
        exit
    }
}

set basequery $query

proc SnapShotBug {id} {
    global log_columns
    SendSQL "select [join $log_columns ","] from bugs where bug_id = $id"
    return [FetchSQLData]
}


foreach id $idlist {
    SendSQL "lock tables bugs write, bugs_activity write, cc write, profiles write"
    set oldvalues [SnapShotBug $id]

    set query "$basequery\nwhere bug_id = $id"
    
# puts "<PRE>$query</PRE>"

    if {![cequal $comma ""]} {
        if { [SendSQL $query] != 0 } {
          puts "<H1>Error -- Changes not applied</H1>"
          puts "OK, the database rejected the changes for some reason"
          puts "which bugzilla can't deal with.  The error string returned"
          puts "was:<PRE>$oramsg(errortxt)</PRE>"
          puts "Here is the query which caused the error:"
          puts "<PRE>$query</PRE>"
        }
        while {[MoreSQLData]} {
            FetchSQLData
        }
    }
    
    if {[info exists FORM(comment)]} {
        AppendComment $id $FORM(who) [FormData comment]
    }
    
    if {[info exists FORM(cc)] && [ShowCcList $id] != [lookup FORM cc]} {
        set ccids(zz) 1
        unset ccids(zz)
        foreach person [split $FORM(cc) " ,"] {
            if {![cequal $person ""]} {
                set cid [DBNameToIdAndCheck $person]
                set ccids($cid) 1
            }
        }
        
        SendSQL "delete from cc where bug_id = $id"
        while {[MoreSQLData]} { FetchSQLData }
        foreach ccid [array names ccids] {
            SendSQL "insert into cc (bug_id, who) values ($id, $ccid)"
            while { [ MoreSQLData ] } { FetchSQLData }
        }
    }

#    oracommit $lhandle

    set newvalues [SnapShotBug $id]
    foreach col $log_columns {
        set old [lvarpop oldvalues]
        set new [lvarpop newvalues]
        if {![cequal $old $new]} {
            if {![info exists whoid]} {
                set whoid [DBNameToIdAndCheck $FORM(who)]
                SendSQL "select delta_ts from bugs where bug_id = $id"
                set timestamp [lindex [FetchSQLData] 0]
            }
            if {[cequal $col assigned_to]} {
                set old [DBID_to_name $old]
                set new [DBID_to_name $new]
            }
            set q "insert into bugs_activity (bug_id,who,when,field,oldvalue,newvalue) values ($id,$whoid,$timestamp,'[SqlQuote $col]','[SqlQuote $old]','[SqlQuote $new]')"
            # puts "<pre>$q</pre>"
            SendSQL $q
        }
    }
    
    puts "<TABLE BORDER=1><TD><H1>Changes Submitted</H1>"
    puts "<TD><A HREF=\"show_bug.cgi?id=$id\">Back To BUG# $id</A></TABLE>"
    flush stdout

    SendSQL "unlock tables"

    exec ./processmail $id < /dev/null > /dev/null 2> /dev/null &
}

if {[info exists next_bug]} {
    set FORM(id) $next_bug
    puts "<HR>"

    navigation_header
    source "bug_form.tcl"
} else {
  puts "<BR><A HREF=\"query.cgi\">Back To Query Page</A>"
}
