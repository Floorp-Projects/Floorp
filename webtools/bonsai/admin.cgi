#!/usr/bonsaitools/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Bonsai CVS tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 

require 'CGI.pl';

use strict;

sub StupidFuncToShutUpWarningsByUsingVarsAgain {
    my $z;
    $z = $::TreeOpen;
    $z = $::CloseTimeStamp;
}

Lock();
LoadCheckins();
LoadMOTD();
LoadTreeConfig();
Unlock();

my $BIP = BatchIdPart('?');
my $BIP_nohook = BatchIdPart();

print "Content-type: text/html\n\n";
PutsHeader("Bonsai Administration  [`$::TreeID' Tree]",
           "Bonsai Administration",
           "Administrating `$::TreeID' Tree");

print <<EOF ;
<pre>
</pre>
<center><b>
You realize, of course, that you have to know the magic password to do
anything from here.
</b></center>
<pre>

</pre>
<hr>
EOF


TweakCheckins();
CloseTree();
TweakTimestamps();
ChangeMOTD();
EditEmailMessage();
RebuildHook();
RebuildHistory();
ChangePasswd();

PutsTrailer();
exit 0;



sub TweakCheckins {
     print qq(

<a href="showcheckins.cgi?tweak=1$BIP_nohook">
    Go tweak bunches of checkins at once.</a><br>
<a href="editparams.cgi">
    Edit Bonsai operating parameters.</a>
<hr>

);
}


sub CloseTree {  # Actually opens tree also
     my $timestamp = value_quote(MyFmtClock(time));

     print qq(

<FORM method=get action=\"doadmin.cgi\">
<B>Password:</B> <INPUT NAME=password TYPE=password> <BR>
<INPUT TYPE=HIDDEN NAME=treeid VALUE=$::TreeID>
);

     if ($::TreeOpen) {
          print qq(
<INPUT TYPE=HIDDEN NAME=command VALUE=close>
<B>Closing time stamp is:</B>
<INPUT NAME=closetimestamp VALUE=\"$timestamp\"><BR>
<INPUT TYPE=SUBMIT VALUE=\"Close the tree\">
);
     } else {
          print qq(
<INPUT TYPE=HIDDEN NAME=command VALUE=open>
<B>The new \"good\" timestamp is:</B>
<INPUT NAME=lastgood VALUE=\"$timestamp\"><BR>
<INPUT TYPE=CHECKBOX NAME=doclear CHECKED>Clear the list of checkins.<BR>
<INPUT TYPE=SUBMIT VALUE=\"Open the tree\">
);
     }

     print qq(</FORM>\n<hr>\n\n);

}


sub TweakTimestamps {
     my $lg_timestamp = value_quote(MyFmtClock($::LastGoodTimeStamp));
     my $c_timestamp = value_quote(MyFmtClock($::CloseTimeStamp));

     print qq(

<FORM method=get action=\"doadmin.cgi\">
<B>Password:</B> <INPUT NAME=password TYPE=password> <BR>
<INPUT TYPE=HIDDEN NAME=treeid VALUE=$::TreeID>
<INPUT TYPE=HIDDEN NAME=command VALUE=tweaktimes>
<TABLE>
<TR>
<TD><B>Last good timestamp:</B></TD>
<TD><INPUT NAME=lastgood VALUE=\"$lg_timestamp\"></TD>
</TR><TR>

<TD><B>Last close timestamp:</B></TD>
<TD><INPUT NAME=lastclose VALUE=\"$c_timestamp\"></TD>
</TR>
</TABLE>
<INPUT TYPE=SUBMIT VALUE=\"Tweak the timestamps\">
</FORM>
<hr>


);
}


sub ChangeMOTD {
     my $motd = value_quote($::MOTD);

     print qq(

<FORM method=get action=\"doadmin.cgi\">
<INPUT TYPE=HIDDEN NAME=treeid VALUE=$::TreeID>
<B>Password:</B> <INPUT NAME=password TYPE=password> <BR>
<INPUT TYPE=HIDDEN NAME=command VALUE=editmotd>

Change the message-of-the-day:<br>
<INPUT TYPE=HIDDEN NAME=origmotd VALUE=\"$motd\">
<TEXTAREA NAME=motd ROWS=10 COLS=50>$::MOTD</TEXTAREA><BR>
<INPUT TYPE=SUBMIT VALUE=\"Change the MOTD\">
</FORM>
<hr>

);
}


sub EditEmailMessage {
     print qq(

<FORM method=get action=\"editmessage.cgi\">
<INPUT TYPE=HIDDEN NAME=treeid VALUE=$::TreeID>

Change the e-mail message sent:
<SELECT NAME=msgname SIZE=1>
<OPTION VALUE=openmessage>when a checkin is made when the tree is open.
<OPTION VALUE=closemessage>when a checkin is made when the tree is closed.
<OPTION VALUE=treeopened>to the hook when the tree opens
<OPTION VALUE=treeopenedsamehook>to the hook when the tree opens and the hook isn\'t cleared
<OPTION VALUE=treeclosed>to the hook when the tree closes
</SELECT><br>
<INPUT TYPE=SUBMIT VALUE=\"Edit a message\">
</FORM>
<hr>

);
}

sub RebuildHook {
     my $lg_timestamp = value_quote(MyFmtClock($::LastGoodTimeStamp));

     print qq(

<FORM method=get action=\"repophook.cgi\">
<INPUT TYPE=HIDDEN NAME=treeid VALUE=$::TreeID>
<B>Password:</B> <INPUT NAME=password TYPE=password> <BR>
<INPUT TYPE=HIDDEN NAME=command VALUE=repophook>
Repopulate the hook from scratch.<p>
<font color=red size=+2>This can be very dangerous.</font>  You should
usually only need to do this to populate a new Bonsai branch.
<p>
<b>Use any checkin since:</b>
<INPUT NAME=startfrom VALUE=\"$lg_timestamp\">
<br>
<INPUT TYPE=SUBMIT VALUE=\"Rebuild the hook\">
</FORM>
<hr>

);
}


sub RebuildHistory {
     my $timestamp = value_quote(MyFmtClock(0));

     print qq(

<FORM method=get action=\"rebuildcvshistory.cgi\">
<INPUT TYPE=HIDDEN NAME=treeid VALUE=$::TreeID>
<B>Password:</B> <INPUT NAME=password TYPE=password> <BR>
<INPUT TYPE=HIDDEN NAME=command VALUE=rebuildcvs>
Recreate the entire list of every checkin ever done to the
$::TreeInfo{$::TreeID}{repository} repository from scratch.
<p>
<font color=red size=+2>This can take an incredibly long time.</font>  You
should
usually only need to do this when first introducing an entire CVS repository
into Bonsai.
<p>
<b>Ignore checkins earlier than:</b>
<INPUT NAME=startfrom VALUE=\"$timestamp\">
<br>
<b>Ignore files before (must be full path starting 
with $::TreeInfo{$::TreeID}{repository}; leave blank to do everything):</b>
<INPUT NAME=firstfile VALUE=\"\" size=50>
<br>
<b>Only do files within the subdirectory of
 $::TreeInfo{$::TreeID}{repository} named:</b>
<INPUT NAME=subdir VALUE=\".\" size=50>
<br>
<INPUT TYPE=SUBMIT VALUE=\"Rebuild cvs history\">
</FORM>
<hr>

);
}

sub ChangePasswd {
     print qq(

<FORM method=post action=\"doadmin.cgi\">
<INPUT TYPE=HIDDEN NAME=treeid VALUE=$::TreeID>
<INPUT TYPE=HIDDEN NAME=command VALUE=changepassword>
Change password.<BR>
<B>Old password:</B> <INPUT NAME=password TYPE=password> <BR>
<B>New password:</B> <INPUT NAME=newpassword TYPE=password> <BR>
<B>Retype new password:</B> <INPUT NAME=newpassword2 TYPE=password> <BR>
<INPUT TYPE=RADIO NAME=doglobal VALUE=0 CHECKED>Change password for this tree<BR>
<INPUT TYPE=RADIO NAME=doglobal VALUE=1>Change master Bonsai password<BR>
<INPUT TYPE=SUBMIT VALUE=\"Change the password\">
</FORM>
);
}
