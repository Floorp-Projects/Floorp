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


<html>
<head>
<title>We don't need no stinkin' HTML compose window</title>
</head>

<body>
<h1>Message editor</h1>"

set filename $FORM(msgname)

set fullfilename [DataDir]/$filename

if {[file exists $fullfilename]} {
    set text [read_file $fullfilename]
} else {
    set text {}
}

puts "
Below is the template for the <b>$filename</b> message.  Type the
magic word and edit at will, but be careful to not break anything,
especially around the headers.

The following magic symbols exist:

<table>"

proc PutDoc {name desc} {
    puts "<tr>"
    puts "<td align=right><tt><b>%$name%</b></tt></td>"
    puts "<td>Replaced by the $desc</td>"
    puts "</tr>"
}


switch -exact -- $filename {
    openmessage -
    closemessage {
        PutDoc name "username of the person getting mail"
        PutDoc dir "directory for this checkin"
        PutDoc files "list of files for this checkin"
        PutDoc log "log message for this checkin"
        PutDoc profile "profile for this user"
    }
    treeopened -
    treeopenedsamehook -
    treeclosed {
        PutDoc "hooklist" "comma-separated list of e-mail address of people on the hook"
    }
    default {
        puts "</table><P><font color=red>Uh, hey, this isn't a legal file for"
        puts "you to be editing here!</font>"
        PutsTrailer
        exit
    }
}

puts "
</TABLE>
<FORM method=get action=\"doeditmessage.cgi\">
<INPUT TYPE=HIDDEN NAME=treeid VALUE=$treeid>
<B>Password:</B> <INPUT NAME=password TYPE=password> <BR>
<INPUT TYPE=HIDDEN NAME=msgname VALUE=$filename>
<INPUT TYPE=HIDDEN NAME=origtext VALUE=\"[value_quote $text]\">
<TEXTAREA NAME=text ROWS=40 COLS=80>$text</TEXTAREA><BR>
<INPUT TYPE=SUBMIT VALUE=\"Change this message\">
</FORM>

 "


PutsTrailer

exit
