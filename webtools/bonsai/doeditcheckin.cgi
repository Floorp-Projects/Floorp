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

use strict;

require 'CGI.pl';

print "Content-type: text/html

<HTML>";

CheckPassword($::FORM{'password'});

Lock();
LoadCheckins();

my $busted = 0;

my $info;

if (!exists $::FORM{'id'}) {
    $busted = 1;
} else {
    $info = eval("\\%" . $::FORM{'id'});
    
    if (!exists $info->{'notes'}) {
        $info->{'notes'} = "";
    }
    
    foreach my $i (sort(keys(%$info))) {
        if (FormData("orig$i") ne $info->{$i}) {
            $busted = 1;
            last;
        }
    }
}

if ($busted) {
    Unlock();
    print "
<TITLE>Oops!</TITLE>
<H1>Someone else has been here!</H1>

It looks like somebody else has changed or deleted this checkin.
Terry was too lazy to implement anything beyond detecting this
condition.  You'd best go start over -- go back to the list of
checkins, look for this checkin again, and decide if you still want to
make your edits.";

    PutsTrailer();
    exit();
}

if (exists $::FORM{'nukeit'}) {
    Log("A checkin for $info->{person} has been nuked.");
} else {
    Log("A checkin for $info->{person} has been modified.");
}

$info->{date} = ParseTimeAndCheck(FormData('datestring'));
foreach my $i ('person', 'dir', 'files', 'notes', 'treeopen', 'log') {
    $info->{$i} = FormData($i);
}

if (exists $::FORM{'nukeit'}) {
    my $w = lsearch(\@::CheckInList, $::FORM{'id'});
    if ($w >= 0) {
        splice(@::CheckInList, $w, 1);
    }
}

WriteCheckins();

print "OK, the checkin has been changed.";

PutsTrailer();

