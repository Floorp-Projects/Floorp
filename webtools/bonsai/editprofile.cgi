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

LoadPerson $FORM(person)

puts "Content-type: text/html

"

NOTDEF {
    if {![cequal $FORM(password) $person(password)]} {
        puts "<TITLE>Bzzzzt!</TITLE>"
        puts "<H1>Invalid password.</h1>"
        puts "Please click the <b>Back</b> button and try again."
        exit
    }
}


puts "
<TITLE>Tell us about yourself...</TITLE>
<H1>Profile for $person(name)</H1>
<FORM method=get action=\"doeditprofile.cgi\">
<INPUT TYPE=HIDDEN NAME=person VALUE=$FORM(person)>
<INPUT TYPE=HIDDEN NAME=name VALUE=$FORM(person)>
<TABLE>
<TR>
<TD><B>Real name:</B></TD>
<TD><INPUT NAME=fullname VALUE=\"[value_quote $person(fullname)]\"></TD>
</TR><TR>
<TD><B>Contact:</B></TD>
<TD>"


set mailmatch 0
if {[regexp -- {^<a href="mailto:(.*)\?subject=(.*)">Send mail to (.*)</a>$} \
         $person(contact) foo name1 subject name2]} {
    if {[cequal $name1 $name2]} {
        set mailmatch 1
    }
}
if {$mailmatch} {
    set checked "CHECKED"
} else {
    set name1 $person(name)
    set subject "Build busted"
    set checked {}
}



puts "
<INPUT TYPE=radio NAME=contacttype VALUE=mailto $checked> Send mail to 
<INPUT NAME=mailtoname VALUE=\"[value_quote $name1]\"> with subject
<INPUT NAME=mailtosubject VALUE=\"[value_quote $subject]\"><BR>"

if {$mailmatch} {
    set checked {}
    set value {}
} else {
    set checked "CHECKED"
    set value $person(contact)
}

puts "
<INPUT TYPE=radio NAME=contacttype VALUE=misc $checked> Other:
<INPUT NAME=misccontact SIZE=70 VALUE=\"[value_quote $value]\"></TD>
</TR><TR>"

NOTDEF {
    puts "
<TD><B>Password:</B></TD>
<TD><INPUT NAME=password TYPE=password VALUE=\"[value_quote $person(password)]\"></TD>
</TR><TR>
<TD><B>Retype password:</B></TD>
<TD><INPUT NAME=password2 TYPE=password VALUE=\"[value_quote $person(password)]\"></TD>
</TR><TR>
}

puts "
</TABLE>
<B>Other info (office phone, home phone numbers, graffiti, whatever):</B>
<BR>
<TEXTAREA NAME=otherinfo ROWS=10 COLS=80>$person(otherinfo)</TEXTAREA>
<BR>
<INPUT TYPE=SUBMIT Value=\"Submit\"></FORM>
"

PutsTrailer

exit
