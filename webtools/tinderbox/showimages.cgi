#!/usr/bonsaitools/bin/perl --
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
# The Original Code is the Tinderbox build tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 


$| = 1;

require 'tbglobals.pl';
require 'imagelog.pl';
require 'header.pl';

# Process the form arguments
%form = ();
&split_cgi_args();

tb_check_password();

print "Content-type: text/html\n\n";

@url = ();
@quote = ();
@width = ();
@height = ();
$i = 0;

EmitHtmlHeader("tinderbox: all images");

print '<UL>
<P>These are all of the images currently in 
<A HREF=http://www.mozilla.org/tinderbox.html>Tinderbox</A>.
<P>Please don\'t give out this URL: this is only here for our debugging
needs, and isn\'t linked to by the rest of Tinderbox: because looking at
all the images at once would be be cheating!  you\'re supposed to let them
surprise you over time.  What, do you read ahead in your desktop calendar,
too?  Where\'s your sense of mystery and anticipation?

<P>
</UL>
';




if ($form{'url'} ne "") {
    $oldname = "$data_dir/imagelog.txt";
    open (OLD, "<$oldname") || die "Oops; can't open imagelog.txt";
    $newname = "$oldname-$$";
    open (NEW, ">$newname") || die "Can't open $newname";
    $foundit = 0;
    while (<OLD>) {
        chop;
        ($url, $width, $height, $quote) = split(/\`/);
        if ($url eq $form{'url'} && $quote eq $form{'origquote'}) {
            $foundit = 1;
            if ($form{'nukeit'} ne "") {
                next;
            }
            $quote = $form{'quote'};
        }
        print NEW "$url`$width`$height`$quote\n";
    }
    close OLD;
    close NEW;
    if (!$foundit) {
        print "<font color=red>Hey, couldn't find it!</font>  Did someone\n";
        print "else already edit it?<P>\n";
        unlink $newname;
    } else {
        print "Change made.<P>";
        rename ($newname, $oldname) || die "Couldn't rename $newname to $oldname";
    }
    $form{'doedit'} = "1";
}

    


$doedit = ($form{'doedit'} ne "");

if (!$doedit) {
    print "
<form method=post>
<input type=hidden name=password value=\"$form{'password'}\">
<input type=hidden name=doedit value=1>
<input type=submit value='Let me edit text or remove pictures.'>
</form><P>";
}



open( IMAGELOG, "<$data_dir/imagelog.txt" ) || die "can't open file";
while( <IMAGELOG> ){
    chop;
    ($url[$i],$width[$i],$height[$i],$quote[$i])  = split(/\`/);
    $i++;
}
close( IMAGELOG );

$i--;
print "<center>";
while( $i >= 0 ){
    $qurl = value_encode($url[$i]);
    $qquote = value_encode($quote[$i]);
    print " 
<img border=2 src='$url[$i]' width='$width[$i]' height='$height[$i]'><br>
<i>$quote[$i]</i>";
    if ($doedit) {
        print "
<form method=post>
<input type=submit name=nukeit value='Delete this image'><br>
<input name=quote size=60 value=\"$qquote\"><br>
<input type=submit name=edit value='Change text'><hr>
<input type=hidden name=url value=\"$qurl\">
<input type=hidden name=origquote value=\"$qquote\">
<input type=hidden name=password value=\"$form{'password'}\">
</form>";
    }
    print "<br><br>\n";
    $i--;
}


