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

Lock
LoadWhiteboard

set oldvalue [FormData origwhite]


if {![cequal $oldvalue $whiteboard]} {
    Unlock
    puts "Content-type: text/html

<TITLE>Error -- pen stolen.</TITLE>
<H1>Someone else just changed the whiteboard.</H1>

Somebody else has changed what's on the whiteboard.  Your changes will
stomp over theirs.
<P>
The whiteboard now reads:
<hr>
<PRE VARIABLE>$whiteboard</PRE>
<hr>
If you really want to change the whiteboard to your text, click the button
below.  Or maybe you want to tweak your text first.  Or you can forget it and
go back to the beginning.

<FORM method=get action=\"doeditwhiteboard.cgi\">
<INPUT TYPE=HIDDEN NAME=origwhite VALUE=\"[value_quote $whiteboard]\">

Change the free-for-all whiteboard:<br>
<TEXTAREA NAME=whiteboard ROWS=10 COLS=70>[FormData whiteboard]</TEXTAREA><BR>
<INPUT TYPE=SUBMIT VALUE=\"Change the Whiteboard\">
</FORM>
"
    PutsTrailer
    exit
}


set newwhiteboard [string trimright [FormData whiteboard]]

MailDiffs "whiteboard" $whiteboard $newwhiteboard

set whiteboard $newwhiteboard
WriteWhiteboard

Unlock

puts "Content-type: text/html

<TITLE>Where's my blue marker?</TITLE>
<H1>The whiteboard has been changed.</H1>
The whiteboard now reads:
<hr>
<PRE VARIABLE>$whiteboard</PRE>
"




Log "Whiteboard changed to be: $whiteboard"

PutsTrailer

exit
