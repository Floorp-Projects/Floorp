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
use Fcntl;

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

if( -r "$tree/ignorebuilds.pl" ){
     require "$tree/ignorebuilds.pl";
 }

print "Content-type: text/html\n\n<HTML>\n";

if( $url = $form{"note"} ){

    $note =~ s/\&/&amp;/gi;
    $note =~ s/\</&lt;/gi;
    $note =~ s/\>/&gt;/gi;
    $enc_note = url_encode( $note );
    lock;
    open( NOTES,">>$tree/notes.txt");
    flock(NOTES, LOCK_EX);
    print NOTES "$buildtime|$buildname|$who|$now|$enc_note\n"; 

       &LoadBuildTable;

    foreach $element (keys %form) {

          if(exists ${$build_name_index}{$element}) {
             print NOTES "${$build_name_index}{$element}|$element|$who|$now|$enc_note\n"; 
          } #EndIf
    } #Endforeach
    close(NOTES);

    print "<H1>The following comment has been added to the log</h1>\n";

    #print "$buildname \n $buildtime \n $errorparser \n $logfile \n  $tree \n $enc_buildname \n";
    print "<pre>\n[<b>$who - $now_str</b>]\n$note\n</pre>";

    print"
<p><a href=\"showlog.cgi?tree=$tree\&buildname=$enc_buildname\&buildtime=$buildtime\&logfile=$logfile\&errorparser=$errorparser\">
Go back to the Error Log</a>
<a href=\"showbuilds.cgi?tree=$tree\">
<br>Go back to the build Page</a>";

} else {

&GetBuildNameIndex;

@names = sort (keys %$build_name_index);

    if( $buildname eq '' || $buildtime == 0 ){
        print "<h1>Invalid parameters</h1>\n";
        die "\n";
    }

#print "$buildname \n $buildtime \n $errorparser \n $logfile \n  $tree \n $enc_buildname \n";

    print "
<title>Add a Comment to the log</title>
<H1>Add a Comment to the log</h1>

<form action='addnote.cgi' METHOD='post'>

<br>Your email address: <INPUT Type='input' name='who' size=32><BR>
<TEXTAREA NAME=note ROWS=10 COLS=30 WRAP=HARD>
</textarea>
<INPUT Type='hidden' name='buildname' value='${buildname}'>
<INPUT Type='hidden' name='buildtime' value='${buildtime}'>
<INPUT Type='hidden' name='errorparser' value='$errorparser'>
<INPUT Type='hidden' name='logfile' value='$logfile'>
<INPUT Type='hidden' name='tree' value='$tree'><BR>

This note will be added to the log selected for <B>$buildname</B>; 
however, you can add a note to the following platforms on this tree (the
most current builds) by selecting the builds below. Note that itallicized
builds are currently 'turned off' on the Tinderbox page for this
tree.<BR>";

for $i (@names){
    if( $i ne "" ){

	if ($i eq $buildname) {
        next;
	}


        print "<INPUT TYPE=checkbox NAME=\"$i\">";

        if (exists ${$ignore_builds}{$i}) {
           print "<I>$i</I><BR>\n";
        } else {
           print "$i<BR>\n";
        } #EndIf
    }
} #Endfor

print "<INPUT Type='submit' name='submit' value='Add Note To Log'><BR>
</form>\n</body>\n</html>";
}

sub GetBuildNameIndex {
local($mailtime, $buildtime, $buildname, $errorparser, $buildstatus, $logfile, $binaryname);

open(BUILDLOG, "$tree/build.dat") or die "Couldn't open build.dat: $!\n";

while(<BUILDLOG>) {
    chomp;
    ($mailtime, $buildtime, $buildname, $errorparser, $buildstatus, $logfile, $binaryname) = 
         split( /\|/ );

    $build_name_index->{$buildname} = 0;

} #EndWhile
close(BUILDLOG);

}


sub LoadBuildTable {
local($mailtime, $buildtime, $buildname, $errorparser, $buildstatus, $logfile, $binaryname);

open(BUILDLOG, "$tree/build.dat") or die "Couldn't open build.dat: $!\n";

while(<BUILDLOG>) {
    chomp;
    ($mailtime, $buildtime, $buildname, $errorparser, $buildstatus, $logfile, $binaryname) = 
         split( /\|/ );

    if ($buildtime > $build_name_index->{$buildname} ) {
    $build_name_index->{$buildname} = $buildtime;
    }

} #EndWhile
close(BUILDLOG);

}

