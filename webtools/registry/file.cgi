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

$filename= $form{"file"};
$dirname= $form{"dir"};
$branch= $form{"branch"};
$cvsroot= $form{"cvsroot"};
$rev= $form{"rev"};
$prev_rev= $form{"prev_rev"};
$linked_text= $form{"linked_text"};
$linked_text = $filename if $linked_text eq '';

$branch = $rev if $branch eq '' && $rev =~ /[A-Za-z]/;

@extra_url = ();
@extra_text = ();

print "Content-type: text/html\n\n<HTML>\n";

print "<table border=1 cellspacing=1 cellpadding=3><tr><td>\n";

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


$dirname2 = $dirname;

$dirname2 =~ s/^ns\///;

print "
$linked_text
<SPACER TYPE=VERTICAL SIZE=5>
<dt><A HREF=../bonsai/cvsblame.cgi?file=$dirname/$filename&rev=$rev&root=$cvsroot>
  View Blame-Annotated Source</A>
";
if ($prev_rev ne '') {
    print "<dt><A HREF='../bonsai/cvsview2.cgi"
        ."?diff_mode=context&whitespace_mode=show"
        ."&root=$cvsroot&subdir=$dirname&command=DIFF_FRAMESET&file=$filename"
        ."&rev1=$rev&rev2=$prev_rev'>View Diff $prev_rev vs. $rev</A>";
} else {
    
    print "<dt><A HREF='../bonsai/cvsview2.cgi?subdir=$dirname"
        ."\&files=$filename\&command=DIRECTORY&branch=$branch&root=$cvsroot'>"
        ."View Diff's</A>";
}

print "<DT><A HREF='../bonsai/cvslog.cgi?file=$dirname/$filename&rev=$rev&root=$cvsroot'>
  View Logs</A>
";

print "
</td></tr></table>";

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
