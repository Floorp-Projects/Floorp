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




if {![info exists FORM(person)]} {
    puts {
<TITLE>Uh, who?</TITLE>
<H1>Who would you like to know more about?</H1>
Please enter the username of the person whose Bonsai profile
you'd like to see.
<p>
<form method=get action="profile.cgi">
<B>User:</B><INPUT SIZE=10 NAME=person>
<INPUT TYPE=SUBMIT Value="Submit"></FORM>
    }
    PutsTrailer
    exit
}


set fid [open "|./data/ldapsearch -b \"o=Netscape Communications Corp.,c=US\" -h $ldapserver -p $ldapport -s sub \"(mail=$FORM(person)@netscape.com)\" cn" r]

while {[gets $fid line] >= 0} {
    if {[regexp -- {^cn: (.*)$} $line foo n]} {
        set fullname $n
    }
}

close $fid

if {![info exists fullname]} {
    puts {
<TITLE>Uh, who?</TITLE>
<H1>Who would you like to know more about?</H1>
There doesn't seem to be anybody with e-mail address 
<b><tt>$FORM(person)</tt></b>.

<p>

Please enter the username of the person whose Bonsai profile
you'd like to see.
<p>
<form method=get action="profile.cgi">
<B>User:</B><INPUT SIZE=10 NAME=person>
<INPUT TYPE=SUBMIT Value="Submit"></FORM>
    }
    PutsTrailer
    exit
}
    



puts "Content-type: text/html
Refresh: 0; URL=http://phonebook/cgi-bin/expand-entry.pl?fullname=[url_quote "$fullname,o=Netscape Communications Corp.,c=US"]

<HTML>
<TITLE>What a hack.</TITLE>
One moment while we whisk you away to the appropriate phonebook page..."



exit
