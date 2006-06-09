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
# The Original Code is the Application Registry.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
&split_cgi_args;
1;

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

sub url_encode {
  my ($s) = @_;

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
