#!/usr/bonsaitools/bin/perl
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
# The Original Code is the Application Registry.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.

require 'lloydcgi.pl';
$|=1;

$email= $form{"email"};
$full_name = $email;
$enc_full_name = $email;    #this should be url encoded

@extra_url = ();
@extra_text = ();

print "Content-type: text/html\n\n<HTML>\n";

print "<table border=1 cellspacing=1 cellpadding=3><tr><td>\n";
print "$email\n";
print "<SPACER TYPE=VERTICAL SIZE=5>\n";

&load_extra_data;


$i = 0;
while( $i < @extra_text ){
    $t = $extra_text[$i];    
    if( $u = $extra_url[$i] ){
        
        print("<dt><a href=$u>$t</a>\n");
    }
    else {
        print("<dt>$t\n");
    }
    $i++;
}

if( @extra_text ){
    print("<hr>\n");
}

print "
<dt><A HREF='mailto:$email\@netscape.com'>
  Send Mail</A>



<dt><A HREF='../bonsai/cvsquery.cgi?module=all&branch=&dir=&file=&who=$email&sortby=Date&hours=2&date=week&mindate=&maxdate='>
  Check-ins within 7 days</A>

</table>

<form method='post' name='aka' action='http://aka/aka/snoop-reverse-cache.cgi'>
<input type=hidden name=alias value='$email'>   </form>
";

sub load_extra_data {
    local( $i, $u, $t );

    $i = 0;
    while( ($u = $form{"u${i}"}) ne "" || $form{"t${i}"} ne "" ) {
        $t = $form{"t${i}"};
        if( $t eq "" ) {$t = $u };
        $extra_url[$i] = $u;
        $extra_text[$i] = $t;
        $i++;
    }
}
