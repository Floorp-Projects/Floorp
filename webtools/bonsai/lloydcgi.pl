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

%form = ();
&split_cgi_args;
1;

sub split_cgi_args {
    local (@args, $pair, $key, $value, $s);

    if ($ENV{"REQUEST_METHOD"} eq 'POST') {
        $s .= $_ while (<>);
    }
    else {
        $s = $ENV{"QUERY_STRING"};
    }

    $s =~ tr/+/ /;
    @args= split(/\&/, $s );

    for $pair (@args) {
        ($key, $value) = split(/=/, $pair);
        $key   =~ s/%([a-fA-F0-9]{2})/pack("C", hex($1))/eg;
        $value =~ s/%([a-fA-F0-9]{2})/pack("C", hex($1))/eg;
        $form{$key} = $value;
    }

    # extract the cookies from the HTTP_COOKIE environment 
    %cookie_jar = split('[;=] *',$ENV{'HTTP_COOKIE'});
}

sub make_cgi_args {
    local($k,$v,$ret);
    for $k (sort keys %form){
        $ret .= ($ret eq "" ? '?' : '&');
        $v = $form{$k};
        $ret .= &url_encode2($k);
        $ret .= '=';
        $ret .= &url_encode2($v);
    }
    return $ret;
}

sub url_encode2 {
    local( $s ) = @_;

    $s =~ s/\%/\%25/g;
    $s =~ s/\=/\%3d/g;
    $s =~ s/\?/\%3f/g;
    $s =~ s/ /\%20/g;
    $s =~ s/\n/\%0a/g;
    $s =~ s/\r//g;
    $s =~ s/\"/\%22/g;
    $s =~ s/\'/\%27/g;
    $s =~ s/\|/\%7c/g;
    $s =~ s/\&/\%26/g;
    $s =~ s/\+/\%2b/g;
    return $s;
}

@weekdays = ('Sun','Mon','Tue','Wed','Thu','Fri','Sat');
@months = ('Jan','Feb','Mar','Apr','May','Jun',
           'Jul','Aug','Sep','Oct','Nov','Dec');

sub toGMTString {
    local ($seconds) = $_[0];

    local ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst)
        = gmtime($seconds);
    $year += 1900;

    sprintf('%s, %02d-%s-%d %02d:%02d:%02d GMT',
            $weekdays[$wday],$mday,$months[$mon],$year,$hour,$min,$sec);
}
