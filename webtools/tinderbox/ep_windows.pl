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
    || $line =~ / error /   # C error
    || $line =~ /^C /   # cvs merge conflict
    || $line =~ /error C/   # C error
    || $line =~ /Creating new precompiled header/   # Wastes time.
    || $line =~ /error:/    # java error
    || $line =~ /jmake.MakerFailedException:/    # java error
    || $line =~ /Unknown host /    # cvs error
    || $line =~ /: build failed\;/    # nmake error
    || ($line =~ /gmake/ && $line =~ / Error /)
    || $line =~ /\[checkout aborted\]/ #cvs error
    || $line =~ /\: cannot find module/ #cvs error
;
}


sub has_warning {
    $line =~ /: warning/  # link error
    || $line =~ / error /   # C error
;
}

sub has_errorline {
    local( $line ) = @_;
    $error_file = ''; #'NS\CMD\WINFE\CXICON.cpp';
    $error_line = 0;

    if( $line =~ m@(ns([\\/][a-z0-9\._]+)*)@i ){
        $error_file = $1;
        $error_file_ref = lc $error_file;
        $error_file_ref =~ s@\\@/@g;

        $line =~ m/\(([0-9]+)\)/;
        $error_line = $1;
        return 1;
    }

    if( $line =~ m@(^([A-Za-z0-9_]+\.[A-Za-z])+\(([0-9]+)\))@ ){
        $error_file = $1;
        $error_file_ref = lc $2;
        $error_line = $3;
        $error_guess=1;
        $error_file_ref =~ s@\\@/@g;

        return 1;
    }

    return 0;
}

