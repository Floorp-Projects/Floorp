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

# Shut up misguided -w warnings about "used only once".  "use vars" just
# doesn't work for me.

sub sillyness {
    my $zz;
    $zz = $::BatchID;
}
require 'CGI.pl';

print "Content-type: text/html

<HTML>";

CheckPassword($::FORM{'password'});

Lock();
LoadCheckins();

if (!exists $::FORM{'command'}) {
    $::FORM{'command'} = 'nocommand';
}


my @list;

foreach my $i (keys %::FORM) {
    my $j = url_decode($i);
    if ($j =~ m/^\:\:checkin_/) {
        if (lsearch(\@::CheckInList, $j) >= 0) {
            push(@list, $j);
        }
    }
}


my $origtree = $::TreeID;

my $what = "";

my $i;

SWITCH: for ($::FORM{'command'}) {
    /^nuke$/ && do {
        foreach $i (@list) {
            my $w = lsearch(\@::CheckInList, $i);
            if ($w >= 0) {
                splice(@::CheckInList, $w, 1);
            }
        }
        $what = "deleted.";
        last SWITCH;
    };
    /^setopen$/ && do {
        foreach $i (@list) {
            my $info = eval("\\%" . $i);
            $info->{'treeopen'} = 1;
        }
        $what = "modified to be open.";
        last SWITCH;
    };

    /^setclose$/ && do {
        foreach $i (@list) {
            my $info = eval("\\%" . $i);
            $info->{'treeopen'} = 0;
        }
        $what = "modified to be closed.";
        last SWITCH;
    };
    /^movetree$/ && do {
        if ($::TreeID eq $::FORM{'desttree'}) {
            print "<H1>Pick a different tree</H1>\n";
            print "You attempted to move checkins into the tree that\n";
            print "they're already in.  Hit <b>Back</b> and try again.\n";
            PutsTrailer();
            exit();
        }
        foreach $i (@list) {
            my $w = lsearch(\@::CheckInList, $i);
            if ($w >= 0) {
                splice(@::CheckInList, $w, 1);
            }
        }
        WriteCheckins();
        undef @::CheckInList;
        $::TreeID = $::FORM{'desttree'};
        undef $::BatchID;
        LoadCheckins();
        LoadTreeConfig();
        foreach $i (@list) {
            push(@::CheckInList, $i);
        }
        $what = "moved to the $::TreeInfo{$::TreeID}->{'description'} tree.";
        last SWITCH;
    };
    # DEFAULT
    print "<h1>No command selected</h1>\n";
    print "You need to select one of the radio command buttons at the\n";
    print "bottom.  Hit <b>Back</b> and try again.\n";
    PutsTrailer();
    exit();
}

WriteCheckins();
Unlock();

print "
<H1>OK, done.</H1>
The selected checkins have been $what
";

$::TreeInfo = $origtree;

PutsTrailer();

