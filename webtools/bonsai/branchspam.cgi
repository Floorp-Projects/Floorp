#!/usr/bonsaitools/bin/perl --
# -*- Mode: perl; indent-tabs-mode: nil -*-
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

# Query the CVS database.
#
$|=1;

print "Content-type: text/html

<HTML>";

require 'modules.pl';


print "
<HEAD>
<TITLE>The CVS Branch Spammer (TM)</TITLE>
</HEAD>
<H1>The CVS Branch Spammer (TM)</H1>
<p> Questions, Comments, Feature requests? mail <a href=mailto:ltabb>ltabb</a>
<h3>What this tool does</h3>

<p>In the course of software development, it is necessary to form a branch
to do development on for a period of time.  Sometimes you want to merge these 
changes back into the trunk in one shot. Sometime you want to have the developers
merge the changes themselves, individually.  This tool makes sure the developers
have merged their changes in individually.

<p>The CVS Branch Spammer goes out and figures out what changes were made on
a branch and then looks to see if these changes where also made on the tip.  It
formulates a mail message and send the mail to the indivual developers.  The 
individual developers look at the mail and reply that they have made their
changes in the tip.

<p>To run this program answer the following questions and bonk the spam button.


<p>
<FORM METHOD=GET ACTION='branchspammer.cgi'>
";


#
# module selector
#
print "

<nobr><b>Pick the name of the CVS Module you use to pull your source</b>
<SELECT name='module' size=5>
<OPTION SELECTED VALUE='all'>All Files in the Repository
<OPTION SELECTED VALUE='Client40All'>Client40All
";

#
# Print out all the Different Modules
#
for $k  (sort( keys( %$modules ) ) ){
    print "<OPTION value='$k'>$k\n";
}

print "</SELECT></NOBR>\n";

#
# Branch
#
print "<br><nobr><b>What is the name of your branch:</b> <input type=text name=branch size=25></nobr>\n";

print "<br><nobr><b>Who should the email message be from?:</b> <input type=text name=whofrom size=25></nobr>\n";

print "
<br>
<br>
<INPUT TYPE=SUBMIT VALUE='Run the Branchspammer'>
</FORM>";

