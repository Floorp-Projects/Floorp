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
puts "Content-type: text/html"
puts ""

ConnectToDatabase


proc Status {str} {
    puts "$str <P>"
    flush stdout
}

proc Alert {str} {
    Status "<font color=red>$str</font>"
}

proc BugLink {id} {
    return "<a href='show_bug.cgi?id=$id'>$id</a>"
}


PutHeader "Bugzilla Sanity Check"  "Bugzilla Sanity Check"

puts "OK, now running sanity checks.<P>"

Status "Checking profile ids..."

SendSQL "select userid,login_name from profiles"

while {[MoreSQLData]} {
    lassign [FetchSQLData] id email
    if {[regexp {^[^@, ]*@[^@, ]*\.[^@, ]*$} $email]} {
        set profid($id) 1
    } else {
        if {$id != ""} {
            Alert "Bad profile id $id &lt;$email&gt;."
        }
    }
}


catch {[unset profid(0)]}


Status "Checking reporter/assigned_to ids"
SendSQL "select bug_id,reporter,assigned_to from bugs"

while {[MoreSQLData]} {
    lassign [FetchSQLData] id reporter assigned_to
    if {$id == ""} {
        continue
    }
    set bugid($id) 1
    if {![info exists profid($reporter)]} {
        Alert "Bad reporter $reporter in [BugLink $id]"
    }
    if {![info exists profid($assigned_to)]} {
        Alert "Bad assigned_to $assigned_to in [BugLink $id]"
    }
}

Status "Checking CC table"

SendSQL "select bug_id,who from cc";
while {[MoreSQLData]} {
    lassign [FetchSQLData] id cc
    if {$cc == ""} {
        continue
    }
    if {![info exists profid($cc)]} {
        Alert "Bad cc $cc in [BugLink $id]"
    }
}


Status "Checking activity table"

SendSQL "select bug_id,who from bugs_activity"

while {[MoreSQLData]} {
    lassign [FetchSQLData] id who
    if {$who == ""} {
        continue
    }
    if {![info exists bugid($id)]} {
        Alert "Bad bugid [BugLink $id]"
    }
    if {![info exists profid($who)]} {
        Alert "Bad who $who in [BugLink $id]"
    }
}
