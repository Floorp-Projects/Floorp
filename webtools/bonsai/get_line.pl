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

# Get a line, dealing with '\'.  Returns 'undef' when no more lines to return;
# removes blank lines as it goes.
# Allows spaces after a '\'.  This is naughty but will probably not matter
# *too* much, and I'm not changing it now.
sub get_line {
    my($l, $save);
    $l='';
    $save='';

    my $bContinue = 1;

    while( $bContinue && ($l = <MOD>) ){
        chop($l);
        if( $l =~ /^[ \t]*\#/
                || $l =~ /^[ \t]*$/ ){
            $l='';              # Starts with a "#", or is only whitespace.
        }
        if( $l =~ /\\[ \t]*$/ ){
            # Ends with a slash, so append it to the last line.
            chop ($l);
            $save .= $l . ' ';
            $l='';
        }
        elsif( $l eq '' && $save eq ''){
            # ignore blank lines
        }
        else {
            $bContinue = 0;
        }
    }
    if(!defined($l)) {
        if($save ne '') {
            return $save;
        } else {
            return $l;
        }
    } else {
        return $save . $l;
    }
}

1;
