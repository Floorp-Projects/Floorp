#!/usr/bonsaitools/bin/mysqltcl
# -*- Mode: tcl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is the Bonsai CVS tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.

source CGI.tcl

puts "Content-type: text/html

"

NOTDEF {
    if {![cequal $FORM(password) $FORM(password2)]} {
        puts {
            <TITLE>Bzzzzt!</TITLE>
            <H1>Passwords don't match.</H1>
            The two password fields in the form don't contain the same thing.  You must
            type the same password in both places.
            <P>
            Please click the <b>Back</b> button and try again.
        }
        exit
    }
}

Lock
LoadPerson $FORM(person)

foreach i [array names person] {
    if {[info exists FORM($i)]} {
        set person($i) [FormData $i]
    }
}




switch $FORM(contacttype) {
    mailto {
        set person(contact) [ConstructMailTo [FormData mailtoname] \
                                 [FormData mailtosubject]]
    }
    misc {
        set person(contact) [FormData misccontact]
    }
}
        



WritePerson $FORM(person)
Unlock

puts "
Your profile has been updated. <hr> [GenerateProfileHTML]"

PutsTrailer

exit
