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

use lib '../bonsai';
require "globals.pl";
require 'lloydcgi.pl';

$| = 1;
print "Content-type: text/html\n\n";

print "<H1>Notes log</H1>\n";

my $filename = "$form{'tree'}/notes.txt";

open(NOTES, "<$filename") || die "Can't open $filename";

my $header = "<table border=1><th>Build time</th><th>Build name</th><th>Who</th><th>Note time</th><th>Note</th>";

print "$header\n";

my $count = 0;
while (<NOTES>) {
    chop;
    my ($nbuildtime,$nbuildname,$nwho,$nnow,$nenc_note) = split /\|/;
    my $note = &url_decode($nenc_note);
    $nbuildtime = print_time($nbuildtime);
    $nnow = print_time($nnow);
    print "<tr>";
    print "<td>$nbuildtime</td>";
    print "<td>$nbuildname</td>";
    print "<td>$nwho</td>";
    print "<td>$nnow</td>";
    print "<td>$note</td>";
    print "</tr>\n";
    if (++$count % 100 == 0) {
        print "</table>$header\n";
    }
}
print "</table>\n";
