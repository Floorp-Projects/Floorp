#!/usr/bonsaitools/bin/perl -w
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

require 'globals.pl';
require 'adminfuncs.pl';

use strict;


sub GetDate {
    my ($line) = (@_);
    my $date;
    if ($line =~ /([0-9].*)$/) {
        $date = str2time($1);
    } else {
        $date = time();
    }
    return $date;
}



Lock();

open(FID, "<$ARGV[0]") || die "Can't open $ARGV[0]";

while (<FID>) {
    chomp();
    my $line = $_;
    if ($line =~ /^([^ ]*)\s+([^ ]*)/) {
        my $foobar = $1;
        $::TreeID = $2;
        $::TreeID = $2;         # Duplicate line to avoid stupid perl warning.
        undef @::CheckInList;
        undef @::CheckInList;   # Duplicate line to avoid stupid perl warning.
        if ($foobar =~ /^opennoclear$/i) {
            LoadCheckins();
            AdminOpenTree(GetDate($line), 0);
            WriteCheckins();
        } elsif ($foobar =~ /^open$/i) {
            LoadCheckins();
            AdminOpenTree(GetDate($line), 1);
            WriteCheckins();
        } elsif ($foobar =~ /^close$/i) {
            LoadCheckins();
            AdminCloseTree(GetDate($line));
            WriteCheckins();
        }
    }
}

Unlock();
