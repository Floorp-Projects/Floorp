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

proc PutsLDAPDisclaimer {} {
    puts "
<hr>
<b> Yes, yes, we <i>will</i> punt all this stuff and use the Directory Server,
but we have some exploring and hacking to do first.</b>  Meanwhile, we at
least try to display the phonebook record below.

"
}



puts "Content-type: text/html

<HTML>"


puts [GenerateProfileHTML $FORM(person)]

puts "
<hr>


To change this profile, "

NOTDEF {
    puts " type in the associated password (if any) and "
}

puts "
click on Edit.

<FORM method=get action=\"editprofile.cgi\" target=_top>
<INPUT TYPE=HIDDEN NAME=person VALUE=$FORM(person)>
"

NOTDEF {
    puts "
<B>Password:</B> <INPUT TYPE=PASSWORD SIZE=10 NAME=password>
"
}

puts "
<INPUT TYPE=SUBMIT Value=\"Edit\"></FORM>
</FORM>

<hr>

<a href=\"profile.cgi\" target=_top>See someone else's profile.</a>

"

PutsLDAPDisclaimer


PutsTrailer

exit
