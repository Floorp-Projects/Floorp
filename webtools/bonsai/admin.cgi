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
LoadCheckins
LoadMOTD
LoadTreeConfig
Unlock

puts "Content-type: text/html


<html>
<head>
<title>Bonsai administration</title>
</head>

<body>
<h1>Bonsai administration</h1>

You realize, of course, that you have to know the magic password to do
anything from here.

<hr>
<a href=showcheckins.cgi?tweak=1[BatchIdPart]>Go tweak bunches of checkins at once.</a>
<hr>

"


puts "
<FORM method=get action=\"doadmin.cgi\">
<B>Password:</B> <INPUT NAME=password TYPE=password> <BR>
<INPUT TYPE=HIDDEN NAME=treeid VALUE=$treeid>
"

if {$treeopen} {
    puts "
<INPUT TYPE=HIDDEN NAME=command VALUE=close>
<B>Closing time stamp is:</B>
<INPUT NAME=closetimestamp VALUE=\"[value_quote [MyFmtClock [getclock]]]\"><BR>
<INPUT TYPE=SUBMIT VALUE=\"Close the tree\">
"
} else {
    puts "
<INPUT TYPE=HIDDEN NAME=command VALUE=open>
<B>The new \"good\" timestamp is:</B>
<INPUT NAME=lastgood VALUE=\"[value_quote [MyFmtClock [getclock]]]\">
<BR>
<INPUT TYPE=CHECKBOX NAME=doclear CHECKED>Clear the list of checkins.<BR>
<INPUT TYPE=SUBMIT VALUE=\"Open the tree\">
"
}
puts "
</FORM>

<hr>

<FORM method=get action=\"doadmin.cgi\">
<B>Password:</B> <INPUT NAME=password TYPE=password> <BR>
<INPUT TYPE=HIDDEN NAME=treeid VALUE=$treeid>
<INPUT TYPE=HIDDEN NAME=command VALUE=tweaktimes>
<TABLE>
<TR>
<TD><B>Last good timestamp:</B></TD>
<TD><INPUT NAME=lastgood VALUE=\"[value_quote [MyFmtClock $lastgoodtimestamp]]\"></TD>
</TR><TR>

<TD><B>Last close timestamp:</B></TD>
<TD><INPUT NAME=lastclose VALUE=\"[value_quote [MyFmtClock $closetimestamp]]\"></TD>
</TR>
</TABLE>
<INPUT TYPE=SUBMIT VALUE=\"Tweak the timestamps\">
</FORM>


<hr>

<FORM method=get action=\"doadmin.cgi\">
<INPUT TYPE=HIDDEN NAME=treeid VALUE=$treeid>
<B>Password:</B> <INPUT NAME=password TYPE=password> <BR>
<INPUT TYPE=HIDDEN NAME=command VALUE=editmotd>

Change the message-of-the-day:<br>
<INPUT TYPE=HIDDEN NAME=origmotd VALUE=\"[value_quote $motd]\">
<TEXTAREA NAME=motd ROWS=10 COLS=50>$motd</TEXTAREA><BR>
<INPUT TYPE=SUBMIT VALUE=\"Change the MOTD\">
</FORM>


<hr>

<FORM method=get action=\"editmessage.cgi\">
<INPUT TYPE=HIDDEN NAME=treeid VALUE=$treeid>

Change the e-mail message sent:
<SELECT NAME=msgname SIZE=1>
<OPTION VALUE=openmessage>when a checkin is made when the tree is open.
<OPTION VALUE=closemessage>when a checkin is made when the tree is closed.
<OPTION VALUE=treeopened>to the hook when the tree opens
<OPTION VALUE=treeopenedsamehook>to the hook when the tree opens and the hook isn't cleared
<OPTION VALUE=treeclosed>to the hook when the tree closes
</SELECT>
<br>
<INPUT TYPE=SUBMIT VALUE=\"Edit a message\">

</FORM>


<hr>

<FORM method=get action=\"repophook.cgi\">
<INPUT TYPE=HIDDEN NAME=treeid VALUE=$treeid>
<B>Password:</B> <INPUT NAME=password TYPE=password> <BR>
<INPUT TYPE=HIDDEN NAME=command VALUE=repophook>
Repopulate the hook from scratch.<p>
<font color=red size=+2>This can be very dangerous.</font>  You should
usually only need to do this to populate a new Bonsai branch.
<p>
<b>Use any checkin since:</b>
<INPUT NAME=startfrom VALUE=\"[value_quote [MyFmtClock $lastgoodtimestamp]]\">
<br>
<INPUT TYPE=SUBMIT VALUE=\"Rebuild the hook\">
</FORM>

<hr>

<FORM method=get action=\"rebuildtaginfo.cgi\">
<INPUT TYPE=HIDDEN NAME=treeid VALUE=$treeid>
<B>Password:</B> <INPUT NAME=password TYPE=password> <BR>
<INPUT TYPE=HIDDEN NAME=command VALUE=rebuildtaginfo>
Recreate the entire table of tags for the $treeinfo($treeid,repository)
repository from scratch.
<p>
<font color=red size=+2>This can take a very, very long time.</font>  You
should
usually only need to do this when first introducing an entire CVS repository
into Bonsai.  (And, in fact, nothing right now ever even uses that info, so
don't bother unless you know what you're doing.)
<br>
<INPUT TYPE=SUBMIT VALUE=\"Rebuild tag information\">
</FORM>

<hr>

<FORM method=get action=\"rebuildcvshistory.cgi\">
<INPUT TYPE=HIDDEN NAME=treeid VALUE=$treeid>
<B>Password:</B> <INPUT NAME=password TYPE=password> <BR>
<INPUT TYPE=HIDDEN NAME=command VALUE=rebuildcvs>
Recreate the entire list of every checkin ever done to the
$treeinfo($treeid,repository) repository from scratch.
<p>
<font color=red size=+2>This can take an incredibly long time.</font>  You
should
usually only need to do this when first introducing an entire CVS repository
into Bonsai.
<p>
<b>Ignore checkins earlier than:</b>
<INPUT NAME=startfrom VALUE=\"[value_quote [MyFmtClock 0]]\">
<br>
<b>Ignore files before (must be full path starting with $treeinfo($treeid,repository); leave blank to do everything):</b>
<INPUT NAME=firstfile VALUE=\"\" size=50>
<br>
<b>Only do files within the subdirectory of $treeinfo($treeid,repository) named:</b>
<INPUT NAME=subdir VALUE=\".\" size=50>
<br>
<INPUT TYPE=SUBMIT VALUE=\"Rebuild cvs history\">
</FORM>

<hr>
<FORM method=post action=\"doadmin.cgi\">
<INPUT TYPE=HIDDEN NAME=treeid VALUE=$treeid>
<INPUT TYPE=HIDDEN NAME=command VALUE=changepassword>
Change password.<BR>
<B>Old password:</B> <INPUT NAME=password TYPE=password> <BR>
<B>New password:</B> <INPUT NAME=newpassword TYPE=password> <BR>
<B>Retype new password:</B> <INPUT NAME=newpassword2 TYPE=password> <BR>
<INPUT TYPE=RADIO NAME=doglobal VALUE=0 CHECKED>Change password for this tree<BR>
<INPUT TYPE=RADIO NAME=doglobal VALUE=1>Change master Bonsai password<BR>
<INPUT TYPE=SUBMIT VALUE=\"Change the password\">
</FORM>
"


PutsTrailer

exit
