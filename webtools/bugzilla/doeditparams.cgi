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


PutHeader "Saving new parameters" "Saving new parameters"

foreach i $param_list {
    if {[info exists FORM(reset-$i)]} {
        set FORM($i) $param_default($i)
    }
    if {![cequal $FORM($i) [Param $i]]} {
        if {![cequal $param_checker($i) ""]} {
            set ok [$param_checker($i) $FORM($i)]
            if {![cequal $ok ""]} {
                puts "New value for $i is invalid: $ok<p>"
                puts "Please hit <b>Back</b> and try again."
                exit
            }
        }
        puts "Changed $i.<br>"
        set param($i) $FORM($i)
    }
}


WriteParams

puts "OK, done.<p>"
puts "<a href=editparams.cgi>Edit the params some more.</a><p>"
puts "<a href=query.cgi>Go back to the query page.</a>"
    
