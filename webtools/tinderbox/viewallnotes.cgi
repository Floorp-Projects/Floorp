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
require 'header.pl';

use Date::Parse;
use Date::Format

my $TIMEFORMAT = "%D %T";

$| = 1;

À€ l print "Content-type: text/html\n\n<HTML>\n";

print "<H1>Notes log</H1>\n";

my $tree = $form{'tree'};
my $start = $form{'start'};
my $end = $form{'end'};

sub str2timeAndCheck {
    my ($str) = (@_);
    my $result = str2time($str);
    if (defined $result && $result > 7000000) {
        return $result;
    }
    print "<p><font color=red>Can't parse as a date: $str</font><p>\n";
    return 0;
}

if (defined $tree && defined $start && defined $end) {
    my $first = str2timeAndCheck($start);
    my $last = str2timeAndCheck($end);
    if ($first > 0 && $last > 0) {
        if (open(IN, "<$tree/notes.txt")) {
            print "<hr><center><h1>Notes for $tree</H1><H3>from " .
                time2str($TIMEFORMAT, $first) . " to " .
                    time2str($TIMEFORMAT, $last) . "</H3></center>\n";
            my %stats;
            while (<IN>) {
                chop;
    		my ($nbuildtime,$nbuildname,$nwho,$nnow,$nenc_note)
		    = split /\|/;
		if ($nbuildtime >= $first && $nbuildtime <= $last) {
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
            }
        } else {
            print "<p><font color=red>There does not appear to be a tree " .
                "named '$tree'.</font><p>";
        }
    }
    print "</table>\n";
}

