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

puts "Set-Cookie: PLATFORM=$FORM(product) ; path=/ ; expires=Sun, 30-Jun-99 00:00:00 GMT"
puts "Set-Cookie: VERSION-$FORM(product)=$FORM(version) ; path=/ ; expires=Sun, 30-Jun-99 00:00:00 GMT"
puts "Content-type: text/html\n"

if {[info exists FORM(maketemplate)]} {
    puts "<TITLE>Bookmarks are your friend.</TITLE>"
    puts "<H1>Template constructed.</H1>"
    
    set url "enter_bug.cgi?$buffer"

    puts "If you put a bookmark <a href=\"$url\">to this link</a>, it will"
    puts "bring up the submit-a-new-bug page with the fields initialized"
    puts "as you've requested."
    exit
}

PutHeader "Posting Bug -- Please wait" "Posting Bug" "One moment please..."

flush stdout
umask 0
ConnectToDatabase

if {![info exists FORM(component)] || [cequal $FORM(component) ""]} {
    puts "You must choose a component that corresponds to this bug.  If"
    puts "necessary, just guess.  But please hit the <B>Back</B> button and"
    puts "choose a component."
    exit 0
}
    

set forceAssignedOK 0
if {[cequal "" $FORM(assigned_to)]} {
    SendSQL "select initialowner from components
where program='[SqlQuote $FORM(product)]'
and value='[SqlQuote $FORM(component)]'"
    set FORM(assigned_to) [lindex [FetchSQLData] 0]
    set forceAssignedOK 1
}

set FORM(assigned_to) [DBNameToIdAndCheck $FORM(assigned_to) $forceAssignedOK]
set FORM(reporter) [DBNameToIdAndCheck $FORM(reporter)]


set bug_fields { reporter product version rep_platform bug_severity \
                     priority op_sys assigned_to bug_status bug_file_loc \
                     short_desc component }
set query "insert into bugs (\n"

foreach field $bug_fields {
  append query "$field,\n"
}

append query "creation_ts, long_desc )\nvalues (\n"


foreach field $bug_fields {
    if {$field == "qa_assigned_to"} {

        set valin [DBname_to_id $FORM($field)]
        if {$valin == "__UNKNOWN__"} {
            append query "null,\n"
        } else {
            append query "$valin,\n"
        }

    } else {
        regsub -all "'" [FormData $field] "''" value
        append query "'$value',\n"
    }
}

append query "now(), "
append query "'[SqlQuote [FormData comment]]' )\n"


set ccids(zz) 1
unset ccids(zz)


if {[info exists FORM(cc)]} {
    foreach person [split $FORM(cc) " ,"] {
        if {![cequal $person ""]} {
            set ccids([DBNameToIdAndCheck $person]) 1
        }
    }
}


# puts "<PRE>$query</PRE>"

SendSQL $query
while {[MoreSQLData]} { set ret [FetchSQLData] }

SendSQL "select LAST_INSERT_ID()"
set id [FetchSQLData]

foreach person [array names ccids] {
    SendSQL "insert into cc (bug_id, who) values ($id, $person)"
    while { [ MoreSQLData ] } { FetchSQLData }
}

# Now make sure changes are written before we run processmail...
Disconnect

puts "<H2>Changes Submitted</H2>"
puts "<A HREF=\"show_bug.cgi?id=$id\">Show BUG# $id</A>"
puts "<BR><A HREF=\"query.cgi\">Back To Query Page</A>"

flush stdout

exec ./processmail $id < /dev/null > /dev/null 2> /dev/null &
exit
