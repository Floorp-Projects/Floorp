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
puts "Content-type: text/html\n"

puts "<HTML>
<H1>Changes made to bug $FORM(id)</H1>
"
set query "
        select bugs_activity.field, bugs_activity.when,
                bugs_activity.oldvalue, bugs_activity.newvalue,
                profiles.login_name
        from bugs_activity,profiles
        where bugs_activity.bug_id = $FORM(id)
        and profiles.userid = bugs_activity.who
        order by bugs_activity.when"

ConnectToDatabase
SendSQL $query

puts "<table border cellpadding=4>"
puts "<tr>"
puts "    <th>Who</th><th>What</th><th>Old value</th><th>New value</th><th>When</th>"
puts "</tr>"

while { [MoreSQLData] } {
    set value [FetchSQLData]
    lassign $value field when old new who

    puts "<tr>"
    puts "<td>$who</td>"
    puts "<td>$field</td>"
    puts "<td>[value_quote $old]</td>"
    puts "<td>[value_quote $new]</td>"
    puts "<td>$when</td>"
    puts "</tr>"
}
puts "</table>"
puts "<hr><a href=show_bug.cgi?id=$FORM(id)>Back to bug $FORM(id)</a>"
