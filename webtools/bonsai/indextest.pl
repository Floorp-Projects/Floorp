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

use strict;

if( $ARGV[0] eq '' ){
    $::CVS_ROOT = '/m/src';
}
else {
    $::CVS_ROOT = $ARGV[0];
}

$CVS_REPOS_SUFIX = $::CVS_ROOT;
$CVS_REPOS_SUFIX =~ s/\//_/g;

my $CHECKIN_DATA_FILE = "data/checkinlog${CVS_REPOS_SUFIX}";
my $CHECKIN_INDEX_FILE = "data/index${CVS_REPOS_SUFIX}";


    open(INDEX , "<$CHECKIN_INDEX_FILE");
    open(CI, "<$CHECKIN_DATA_FILE") || die "could not open checkin data file\n";

    while( <INDEX> ){
        chop;
        ($o,$d) = split(/\|/);
        seek(CI, $o, 0);
        $line = <CI>;
        ($j,$d1) = split(/\|/);
        print "$d|$d1\n";
    }

