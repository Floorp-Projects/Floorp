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
# The Original Code is the Tinderbox build tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.

use lib "../bonsai";

require "globals.pl";
require 'lloydcgi.pl';

$buildname = $form{'buildname'};
$buildtime = $form{'buildtime'};
$errorparser = $form{'errorparser'};
$logfile = $form{'logfile'};
$tree = $form{'tree'};
$enc_buildname = &url_encode($buildname);
$note = $form{'note'};
$who = $form{'who'};
$now = time;
$now_str = &print_time($now);

$|=1;

print "Content-type: text/html\n\n<HTML>\n";

if( $url = $form{"note"} ){

    $enc_note = url_encode( $note );
    lock;
    open( NOTES,">>$tree/notes.txt");
    print NOTES "$buildtime|$buildname|$who|$now|$enc_note\n";
    close(NOTES);
    
    print "<H1>The following comment has been added to the log</h1>\n";

    #print "$buildname \n $buildtime \n $errorparser \n $logfile \n  $tree \n $enc_buildname \n";
    print "<pre>\n[<b>$who - $now_str</b>]\n$note\n</pre>";

    print"
<p><a href=\"showlog.cgi?tree=$tree\&buildname=$enc_buildname\&buildtime=$buildtime\&logfile=$logfile\&errorparser=$errorparser\">
Go back to the Error Log</a>
<a href=\"showbuilds.cgi?tree=$tree\">
<br>Go back to the build Page</a>
";
    
}
else {
    if( $buildname eq '' || $buildtime == 0 ){
        print "<h1>Invalid parameters</h1>\n";
        die "\n";
    }

#print "$buildname \n $buildtime \n $errorparser \n $logfile \n  $tree \n $enc_buildname \n";

    print "
<title>Add a Comment to the log</title>
<H1>Add a Comment to the log</h1>

<form action='addnote.cgi' METHOD='post'>

<br>Your email address: <INPUT Type='input' name='who' size=10>
<TEXTAREA NAME=note ROWS=10 COLS=70>
</textarea>
<INPUT Type='hidden' name='buildname' value='${buildname}'>
<INPUT Type='hidden' name='buildtime' value='${buildtime}'>
<INPUT Type='hidden' name='errorparser' value='$errorparser'>
<INPUT Type='hidden' name='logfile' value='$logfile'>
<INPUT Type='hidden' name='tree' value='$tree'>
<INPUT Type='submit' name='submit' value='Add Note To Log'>
</form>
";
}
