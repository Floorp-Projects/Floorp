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

CheckPassword $FORM(password)


set filename $FORM(msgname)

set fullfilename [DataDir]/$filename

Lock

if {[file exists $fullfilename]} {
    set text [read_file $fullfilename]
} else {
    set text {}
}

if {![cequal [FormData origtext] $text]} {
    puts "
<TITLE>Oops!</TITLE>
<H1>Someone else has been here!</H1>

It looks like somebody else has changed this message while you were editing it.
Terry was too lazy to implement anything beyond detecting this
condition.  You'd best go start over -- go back to the top of Bonsai,
work your way back to editing the message, and decide if you still
want to make your edits."
    PutsTrailer
    exit
}


set text [FormData text]
set fid [open $fullfilename "w"]
puts $fid $text
catch {chmod 0666 $fullfilename }
close $fid


Log "$filename set to $text"

Unlock

        puts "
<TITLE>New $filename</TITLE>
<H1>The file <b>$filename</b> has been changed.</H1>
"

PutsTrailer

exit
