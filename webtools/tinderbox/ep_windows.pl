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

1;
# 
# Scan a line and see if it has an error
#
sub has_error {
  local $_ = $_[0];
  /fatal error/  # . . . . . . . . . . . . . Link error
    or / error / # . . . . . . . . . . . . . C error
    or /error C/ # . . . . . . . . . . . . . C error
    or /Creating new precompiled header/ # . Wastes time
    or /error:/  # . . . . . . . . . . . . . Java error
    or /jmake.MakerFailedException:/ # . . . Java error
    or /: build failed\;/  # . . . . . . . . nmake error
    or /^C / # . . . . . . . . . . . . . . . cvs merge conflict
    or /Unknown host / # . . . . . . . . . . cvs error
    or /\[checkout aborted\]/  # . . . . . . cvs error
    or /\: cannot find module/ # . . . . . . cvs error
    ;
}


sub has_warning {
  local $_ = $_[0];
  /: warning/    # Link error
    or / error / # C error
    ;
}

sub has_errorline {
  local $_ = $_[0];
  my $out  = $_[1];

  $out->{error_line} = 0;

  if(/(mozilla([\\\/][-a-z0-9\._]+)*)/i) {
    $out->{error_file}     = $1;
    $out->{error_file_ref} = $1;
    $out->{error_file_ref} =~ s|\\|/|g;

    /\(([0-9]+)\)/ and $out->{error_line} = $1;

    return 1;
  }

  if( $line =~ m@(^([A-Za-z0-9_]+\.[A-Za-z])+\(([0-9]+)\))@ ){
    $out->{error_file}     = $1;
    $out->{error_file_ref} = $2;
    $out->{error_line}     = $3;
    $out->{error_guess}    = 1;
    $out->{error_file_ref} =~ s@\\@/@g;
    
    return 1;
  }

  return 0;
}

