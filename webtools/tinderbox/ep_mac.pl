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
    $line =~ /fatal error/  # link error
    || $line =~ /Error  /   # C error
    || $line =~ /\[checkout aborted\]/ #cvs error
    || $line =~ /Couldn\'t find project file / # CW project error
    || $line =~ /Can\'t create / # CW project error
    || $line =~ /Can\'t open / # CW project error
    || $line =~ /Can\'t find / # CW project error
;
}

sub has_warning {                                    
    $line =~ /^[A-Za-z0-9_]+\.[A-Za-z0-9]+\ line [0-9]+/ ;
}

sub has_errorline {
    local( $line ) = @_;
    if( $line =~ /^(([A-Za-z0-9_]+\.[A-Za-z0-9]+) line ([0-9]+))/ ){
        $error_file = $1;
        $error_file_ref = $2;
        $error_line = $3;
        $error_guess = 1;
        return 1;
    }
    return 0;
}
