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
source "defparams.tcl"

confirm_login

puts "Content-type: text/html\n"

if {![cequal [Param "maintainer"] $COOKIE(Bugzilla_login)]} {
    puts "<H1>Sorry, you aren't the maintainer of this system.</H1>"
    puts "And so, you aren't allowed to edit the parameters of it."
    exit
}


PutHeader "Edit parameters" "Edit parameters"

puts "This lets you edit the basic operating parameters of bugzilla.  Be careful!"
puts "<p>"
puts "Any item you check Reset on will get reset to its default value."

puts "<form method=post action=doeditparams.cgi><table>"

set rowbreak "<tr><td colspan=2><hr></td></tr>"
puts $rowbreak

foreach i $param_list {
    puts "<tr><th align=right valign=top>$i:</th><td>$param_desc($i)</td></tr>"
    puts "<tr><td valign=top><input type=checkbox name=reset-$i>Reset</td><td>"
    set value [Param $i]
    switch $param_type($i) {
	t {
	    puts "<input size=80 name=$i value=\"[value_quote $value]\">"
	}
	l {
	    puts "<textarea wrap=hard name=$i rows=10 cols=80>[value_quote $value]</textarea>"
	}
        b {
            if {$value} {
                set on "checked"
                set off ""
            } else {
                set on ""
                set off "checked"
            }
            puts "<input type=radio name=$i value=1 $on>On "
            puts "<input type=radio name=$i value=0 $off>Off"
        }
        default {
            puts "<font color=red><blink>Unknown param type $param_type($i)!!!</blink></font>"
        }
    }
    puts "</td></tr>"
    puts $rowbreak
}

puts "</table>"

puts "<input type=reset value=\"Reset form\"><br>"
puts "<input type=submit value=\"Submit changes\">"

puts "</form>"

puts "<p><a href=query.cgi>Skip all this, and go back to the query page</a>"
