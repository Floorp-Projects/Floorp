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

# The master list not only says what fields are possible, but what order
# they get displayed in.

set masterlist {opendate changeddate severity priority platform owner reporter status
    resolution component product version project os summary summaryfull }


if {[info exists FORM(rememberedquery)]} {
    if {[info exists FORM(resetit)]} {
        set collist $default_column_list
    } else {
        set collist {}
        foreach i $masterlist {
            if {[info exists FORM(column_$i)]} {
                lappend collist $i
            }
        }
    }
    puts "Set-Cookie: COLUMNLIST=$collist ; path=/ ; expires=Sun, 30-Jun-99 00:00:00 GMT"
    puts "Refresh: 0; URL=buglist.cgi?$FORM(rememberedquery)"
    puts ""
    puts "<TITLE>What a hack.</TITLE>"
    puts "Resubmitting your query with new columns..."
    exit
}

if {[info exists COOKIE(COLUMNLIST)]} {
    set collist $COOKIE(COLUMNLIST)
} else {
    set collist $default_column_list
}

foreach i $masterlist {
    set desc($i) $i
}

set desc(summary) "Summary (first 60 characters)"
set desc(summaryfull) "Full Summary"


puts ""
puts "Check which columns you wish to appear on the list, and then click on"
puts "submit."
puts "<p>"
puts "<FORM ACTION=colchange.cgi>"
puts "<INPUT TYPE=HIDDEN NAME=rememberedquery VALUE=$buffer>"

foreach i $masterlist {
    if {[lsearch $collist $i] >= 0} {
        set c CHECKED
    } else {
        set c ""
    }
    puts "<INPUT TYPE=checkbox NAME=column_$i $c>$desc($i)<br>"
}
puts "<P>"
puts "<INPUT TYPE=\"submit\" VALUE=\"Submit\">"
puts "</FORM>"
puts "<FORM ACTION=colchange.cgi>"
puts "<INPUT TYPE=HIDDEN NAME=rememberedquery VALUE=$buffer>"
puts "<INPUT TYPE=HIDDEN NAME=resetit VALUE=1>"
puts "<INPUT TYPE=\"submit\" VALUE=\"Reset to Bugzilla default\">"
puts "</FORM>"
