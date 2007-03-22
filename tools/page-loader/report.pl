#!/usr/bin/perl
# 
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla page-loader test, released Aug 5, 2001.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 2001
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   John Morrison <jrgm@netscape.com>, original author
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****
use CGI::Carp qw(fatalsToBrowser);
use CGI::Request;
use URLTimingDataSet;
use strict;

my $request = new CGI::Request;
my $id = $request->param('id'); #XXX need to check for valid parameter id

print "Content-type: text/html\n\n";

print "<p>See Notes at the bottom of this page for some details.</p>\n";
print "<pre>\n";
my $rs = URLTimingDataSet->new($id);

print  "Test id: $id<br>Avg. Median : <b>", $rs->{avgmedian},
       "</b> msec\t\tMinimum     : ", $rs->{minimum}, " msec\n";
print  "Average     : ", $rs->{average},
       " msec\t\tMaximum     : ", $rs->{maximum}, " msec</pre>\n\n\n";

#XXX print more info (test id, ua, start time, user, IP, etc.)

# draw the chart sorted
# XXX enable this line to draw a chart, sorted by time. However, in order
# to draw the chart, you will need to have installed the 'gd' drawing library,
# and the GD and GD::Graph Perl modules.
###print "\n<p><img src='graph.pl?id=", $id, "' height='720' width='800'></p><br>\n";


print "<hr><pre>\nIDX PATH                           AVG    MED    MAX    MIN  TIMES ...\n";

if ($request->param('sort')) {
    print $rs->as_string_sorted();
} else {
    print $rs->as_string();
}
print "</pre>\n";
printEndNotes();

exit;


sub printEndNotes {
    print <<"EndOfNotes";

<hr>
<p>
<ol>
<li>Times are in milliseconds.

<li>AVG, MED, MIN and MAX are the average, median, maximum and
minimum of the (non-NaN) test results for a given page.

<li>If a page fails to fire the onload event within 30 seconds,
the test for that page is "aborted": a different JS function kicks in,
cleans up, and reports the time as "NaN". (The rest of the pages in
the series should still be loaded normally after this).

<li>The value for AVG reported for 'All Pages' is the average of
the averages for all the pages loaded.

<li>The value for MAX and MIN reported for 'All Pages' are the
overall maximum and minimum for all pages loaded (keeping in mind that
a load that never finishes is recorded as "NaN".)

<li>The value for MED reported for 'All Pages' is the _average_ of
the medians for all the pages loaded (i.e., it is not the median of
the medians).

</ol>

</p>
EndOfNotes
}
