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
# The Original Code is the Tinderbox build tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 

require 'tbglobals.pl';
require 'header.pl';

use Date::Parse;
use Date::Format;

my $TIMEFORMAT = "%D %T";

# Process the form arguments
%form = ();
&split_cgi_args();

$|=1;

print "Content-type: text/html\n\n<HTML>\n";

EmitHtmlHeader("Statistics");

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
        if (open(IN, "<$tree/build.dat")) {
            print "<hr><center><h1>Bustage stats for $tree</H1><H3>from " .
                time2str($TIMEFORMAT, $first) . " to " .
                    time2str($TIMEFORMAT, $last) . "</H3></center>\n";
            my %stats;
            while (<IN>) {
                chomp;
                my ($mailtime, $buildtime, $buildname, $errorparser,
                    $buildstatus, $logfile, $binaryurl) = 
                        split( /\|/ );
                if ($buildtime >= $first && $buildtime <= $last) {
                    if (!defined $stats{$buildname}) {
                        $stats{$buildname} = 0;
                    }
                    if ($buildstatus eq "busted") {
                        $stats{$buildname}++;
                    }
                }
            }
            print "<table>\n";
            print "<tr><th>Build name</th><th>Number of bustages</th></tr>\n";
            foreach my $key (sort (keys %stats)) {
                print "<tr><td>$key</td><td>$stats{$key}</td></tr>\n";
            }
            print "</table>\n";
        } else {
            print "<p><font color=red>There does not appear to be a tree " .
                "named '$tree'.</font><p>";
        }
        
    }
    print "<hr>\n";
}

if (!defined $tree) {
    $tree = "";
}

if (!defined $start) {
    $start = time2str($TIMEFORMAT, time() - 7*24*60*60); # One week ago.
}

if (!defined $end) {
    $end = time2str($TIMEFORMAT, time()); # #now
}


print qq|
<form>
<table>
<tr>
<th align=right>Tree:</th>
<td><input name=tree size=30 value="$tree"></td>
</tr>
<tr>
<th align=right>Start time:</th>
<td><input name=start size=30 value="$start"></td>
</tr>
<tr>
<th align=right>End time:</th>
<td><input name=end size=30 value="$end"></td>
</tr>
</table>

<INPUT TYPE=\"submit\" VALUE=\"Generate stats \">

</form>
|;
