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


#
# Multi file diff cgi
#

require 'utils.pl';

$|=1;

print "Content-type: text/html

<PRE><FONT FACE='Lucida Console'>
";

@revs = ();

#if( $ENV{"QUERY_STRING"} eq "" ){
#    $ENV{"QUERY_STRING"}="brendan%2Cns%2Fjs%2Fsrc%2Cjsapi.c%2C-1=on&brendan%2Cns%2Fjs%2Fsrc%2Cjsapi.h%2C-1=on&brendan%2Cns%2Fjs%2Fsrc%2Cjsarray.c%2C-106=on&brendan%2Cns%2Fjs%2Fsrc%2Cjsarray.h%2C-0=on&brendan%2Cns%2Fjs%2Fsrc%2Cjsatom.c%2C-9=on";
#}

&split_cgi_args;

#while( ($k,$v) = each(%ENV) ){
#    print "$k='$v'\n";
#}

if( $form{"cvsroot"} ne "" ){
    $cvsroot = $form{"cvsroot"};
}
else {
    $cvsroot = pickDefaultRepository();
}

if( $form{"allchanges"} ne "" ){
    @revs = split(/,/, $form{"allchanges"} );
}
else {
    while( ($k, $v) = each( %form ) ){
        push( @revs, $k );
    }
}

$didone = 0;
for $k (@revs) {
    ($who,$dir,$file,$rev) = split(/\|/, $k );
    if ($rev eq "") {
        next;
    }
    $prevrev = &PrevRev($rev);
    
    # this doesn't handle files in the attic
    open( DIFF, "/tools/ns/bin/rcsdiff -c -r$prevrev -r$rev $cvsroot/$dir/$file,v 2>&1|" );
    $_ =~ s/&/&amp;/g;
    $_ =~ s/</&lt;/g;
    while(<DIFF>){
        print "$who:  $_";
    }
    $didone = 1;
}

if ($didone == 0) {
    print "No changes were selected.  Please press <b>Back</b> and try again.\n";
}



sub split_cgi_args {
    local($i,$var,$value, $s);

    if( $ENV{"REQUEST_METHOD"} eq 'POST'){
        while(<> ){
            $s .= $_;
        }
    }
    else {
        $s = $ENV{"QUERY_STRING"};
    }

    @args= split(/\&/, $s );

    for $i (@args) {
        ($var, $value) = split(/=/, $i);
        $var =~ tr/+/ /;
        $var =~ s/%([a-fA-F0-9][a-fA-F0-9])/pack("C", hex($1))/eg;
        $value =~ tr/+/ /;
        $value =~ s/%([a-fA-F0-9][a-fA-F0-9])/pack("C", hex($1))/eg;
        $form{$var} = $value;
    }
}

sub PrevRev {
    local( $rev ) = @_;
    local( $i, $j, $ret, @r );

    @r = split( /\./, $rev );

    $i = @r-1;
    
    $r[$i]--;
    if( $r[$i] == 0 ){
        $i -= 2;
    }

    $j = 0;
    while( $j < $i ){
        $ret .= "$r[$j]\.";
        $j++
    }
    $ret .= $r[$i];
}
