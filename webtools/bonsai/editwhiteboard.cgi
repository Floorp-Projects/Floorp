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

LoadWhiteboard

puts "Content-type: text/html

<TITLE>Scritch, scritch.</TITLE>
<FORM method=post action=\"doeditwhiteboard.cgi\">
<INPUT TYPE=HIDDEN NAME=treeid VALUE=$treeid>
<INPUT TYPE=HIDDEN NAME=origwhite VALUE=\"[value_quote $whiteboard]\">

The free-for-all whiteboard is a fine place to put notes of general
and temporary interest about the tree.  (Like, \"I'm checking in a bunch
of nasty stuff; stay out of the tree until 3:30pm\".)

<P>

Change the free-for-all whiteboard:<br>
<TEXTAREA NAME=whiteboard ROWS=10 COLS=70>$whiteboard</TEXTAREA><BR>
<INPUT TYPE=SUBMIT VALUE=\"Change the Whiteboard\">
</FORM>
"

PutsTrailer

exit
