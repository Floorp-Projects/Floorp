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
# The Original Code is the Tinderbox build tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.

use lib "../bonsai";

require 'globals.pl';

$F_DEBUG=1;


$tree = $ARGV[0];

open(SEMFILE, ">>$tree/buildwho.sem") || die "Couldn't open semaphore file!";
if (!flock(SEMFILE, 2 + 4)) {   # 2 means "lock"; 4 means "fail immediately if
                                # lock already taken".
    print "buildwho.pl: Another process is currently building the database.\n";
    exit(0);
}

require "$tree/treedata.pl";

if( $cvs_root eq '' ){
    $CVS_ROOT = '/m/src';
}
else {
    $CVS_ROOT = $cvs_root;
}

$CVS_REPOS_SUFIX = $CVS_ROOT;
$CVS_REPOS_SUFIX =~ s/\//_/g;
    
$CHECKIN_DATA_FILE = "/d/webdocs/projects/bonsai/data/checkinlog${CVS_REPOS_SUFIX}";
$CHECKIN_INDEX_FILE = "/d/webdocs/projects/bonsai/data/index${CVS_REPOS_SUFIX}";

require 'cvsquery.pl';

print "cvsroot='$CVS_ROOT'\n";

&build_who;

flock(SEMFILE, 8);              # '8' is magic 'unlock' const.
close SEMFILE;


sub build_who {
    open(BUILDLOG, "<$tree/build.dat" );
    $line = <BUILDLOG>;
    close(BUILDLOG);

    #($j,$query_date_min) = split(/\|/, $line);
    $query_date_min = time - (60 * 60 * 40);

    if( $F_DEBUG ){
        print "Minimum date: $query_date_min\n";
    }

    $query_module=$cvs_module;
    $query_branch=$cvs_branch;

    $result = &query_checkins;

    
    $last_who='';
    $last_date=0;
    open(WHOLOG, ">$tree/who.dat" );
    for $ci (@$result) {
        if( $ci->[$CI_DATE] != $last_date || $ci->[$CI_WHO] != $last_who ){
            print WHOLOG "$ci->[$CI_DATE]|$ci->[$CI_WHO]\n";
        }
        $last_who=$ci->[$CI_WHO];
        $last_date=$ci->[$CI_DATE];
    }
    close( WHOLOG );
}
