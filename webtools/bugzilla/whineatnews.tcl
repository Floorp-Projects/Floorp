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


# This is a script suitable for running once a day from a cron job.  It 
# looks at all the bugs, and sends whiny mail to anyone who has a bug 
# assigned to them that has status NEW that has not been touched for
# more than 7 days.

source "globals.tcl"
source "defparams.tcl"

ConnectToDatabase

SendSQL "select bug_id,login_name from bugs,profiles where bug_status = 'NEW' and to_days(now()) - to_days(delta_ts) > [Param whinedays] and userid=assigned_to"

set bugs(zz) x
unset bugs(zz)

while {[MoreSQLData]} {
    lassign [FetchSQLData] id email
    if {$id == ""} {
        continue
    }
    lappend bugs($email) $id
}


set template [Param whinemail]


foreach i $param_list {
    regsub -all "%$i%" $template [Param $i] template
}


foreach email [lsort [array names bugs]] {
    
    regsub -all {%email%} $template $email msg
    set list [lsort -integer $bugs($email)]
    foreach i $list {
        append msg "  http://cvs-mirror.mozilla.org/webtools/bugzilla/show_bug.cgi?id=$i\n"
    }
    exec /usr/lib/sendmail -t << $msg
    puts "$email      $list"
}
