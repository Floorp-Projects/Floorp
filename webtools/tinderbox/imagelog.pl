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

1;

sub add_imagelog {
    local($url,$quote,$width,$height) = @_;
    open( IMAGELOG, ">>$data_dir/imagelog.txt" ) || die "Oops; can't open imagelog.txt";
    print IMAGELOG "$url`$width`$height`$quote\n";
    close( IMAGELOG );
}

sub get_image{
    local(@log,@ret,$i);

    open( IMAGELOG, "<$data_dir/imagelog.txt" );
    @log = <IMAGELOG>;

    # return a random line
    srand;
    @ret = split(/\`/,$log[rand @log]);

    close( IMAGELOG );
    @ret;
}


