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
    $zz = $::TreeID;
    $zz = $::TreeInfo;
}

require 'CGI.pl';

print "Content-type: text/html\n\n";

CheckPassword(FormData('password'));
my $Filename = FormData('msgname');
my $RealFilename = DataDir() . "/$Filename";
Lock();

my $Text = '';
if (-f $RealFilename) {
    open(FILE, $ReadFilename);
    while (<FILE>) {
        $Text .= $_;
    }
    close(FILE);
}

unless (FormData('origtext') eq $Text) {
     PutsHeader("Oops!", "Oops!", "Someone else has been here!");
     print "
It looks like somebody else has changed this message while you were editing it.
Terry was too lazy to implement anything beyond detecting this
condition.  You'd best go start over -- go back to the top of Bonsai,
work your way back to editing the message, and decide if you still
want to make your edits.";

     PutsTrailer();
     exit 0;
}

$Text = FormData('text');
open(FILE, "> $RealFilename")
     or warn "Unable to open: $RealFilename: $!\n";
print FILE $Text;
chmod(0666, $RealFilename);
close(FILE);
Log("$RealFilename set to $Text");
Unlock();

LoadTreeConfig();
PutsHeader("New $Filename", "New $Filename",
           "$Filename - $::TreeInfo{$::TreeID}{shortdesc}");
print "The file <b>$Filename</b> has been changed.";
PutsTrailer();
