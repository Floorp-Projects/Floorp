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

use diagnostics;
use strict;

# Shut up misguided -w warnings about "used only once".  "use vars" just
# doesn't work for me.

sub sillyness {
    my $zz;
    $zz = $::TreeInfo;
}

require 'CGI.pl';

print "Content-type: text/html\n\n";

my $Filename = FormData('msgname');
my $RealFilename = DataDir() . "/$Filename";

my $Text = '';
if (-f $RealFilename) {
    open(FILE, $ReadFilename);
    while (<FILE>) {
        $Text .= $_;
    }
    close(FILE);
}

LoadTreeConfig();
PutsHeader("Message Editor", "Message Editor",
           "$Filename - $::TreeInfo{$::TreeID}{shortdesc}");

print "
Below is the template for the <b>$Filename</b> message.  Type the
magic word and edit at will, but be careful to not break anything,
especially around the headers.

The following magic symbols exist:

<table>
";


sub PutDoc {
     my ($name, $desc) = @_;

     print "\n<tr>\n<td align=right><tt><b>%$name%</b></tt></td>
<td>Replaced by the $desc</td>\n</tr>\n";
}

if (($Filename eq 'openmessage') || ($Filename eq 'closemessage')) {
     PutDoc('name', "username of the person getting mail");
     PutDoc('dir', "directory for this checkin");
     PutDoc('files', "list of files for this checkin");
     PutDoc('log', "log message for this checkin");
     PutDoc('profile', "profile for this user");
} elsif (($Filename eq 'treeopened') || ($Filename eq 'treeopenedsamehook') ||
         ($Filename eq 'treeclosed')) {
     PutDoc('hooklist', "comma-separated list of e-mail address of people on the hook");
} else {
     print "</table><P><font color=red>
Uh, hey, this isn't a legal file for you to be editing here!</font>\n";
     PutsTrailer();
     exit 0;
}

print "
</TABLE>
<FORM method=get action=\"doeditmessage.cgi\">
<INPUT TYPE=HIDDEN NAME=treeid VALUE=$::TreeID>
<B>Password:</B> <INPUT NAME=password TYPE=password> <BR>
<INPUT TYPE=HIDDEN NAME=msgname VALUE=$Filename>
<INPUT TYPE=HIDDEN NAME=origtext VALUE=\"" . value_quote($Text) . "\">
<TEXTAREA NAME=text ROWS=40 COLS=80>$Text</TEXTAREA><BR>
<INPUT TYPE=SUBMIT VALUE=\"Change this message\">
</FORM>

 ";


PutsTrailer();
exit 0;

