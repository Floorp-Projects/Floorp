#!/usr/bonsaitools/bin/perl --
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
    

$FILE_LIST = "/d/webdocs/projects/bonsai/data/reposfiles${CVS_REPOS_SUFIX}";

open FL, ">$FILE_LIST";

GoDir($::CVS_ROOT);

sub GoDir {
    local($dir) = @_;
    local(@dirs, $i);

    chdir "$dir";

    while(<*> ){
        if( $_ ne '.' && $_ ne '..' ){
            if( -d $_ ) {
                push @dirs, $_;
            }
            else {
                print FL "$dir/$_\n";
            }
        }
    }

    for $i (@dirs) {
        GoDir( "$dir/$i");
    }
}
