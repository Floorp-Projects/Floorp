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
puts "Content-type: text/plain\n"

set query "update bugs\nset 
bug_status=RESOLVED,
bug_resolution=FIXED
where bug_id = $FORM(id)"

set newcomment "Fixed by changes in $FORM(directory):"
foreach i $FORM(fileversions) {
    lassign $i file version
    append newcomment "\n  $file ($version)"
}

puts "Query is $query"
puts "Comment is $newcomment"

exit

ConnectToDatabase

SendSQL $query

while {[MoreSQLData]} {
  FetchSQLData
  set result [MoreSQLData]
}

AppendComment $FORM(id) $FORM(who) $newcomment


exec ./processmail $FORM(id) < /dev/null > /dev/null 2> /dev/null &
