#!/usr/bin/perl -w
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
# The Original Code is Mozilla Communicator client code.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
#
# $Id: leaks.cgi,v 1.2 1999/11/16 22:10:10 waterson%netscape.com Exp $
#

#
# Expands a logfile into all of the leakers
#

use 5.004;
use strict;
use CGI;
use POSIX;
use Zip;

$::query = new CGI();

# The ZIP where all the log files are kept
$::log = $::query->param('log');
$::zip = new Zip($::log);

print $::query->header;

{
    my @statinfo = stat($::log);
    my $when = POSIX::strftime "%a %b %e %H:%M:%S %Y", localtime($statinfo[9]);

    print $::query->start_html("Leaked Objects, $when"),
          $::query->h1("Leaked Objects");

    print "<small>$when<br><a href='bloat-log.cgi?log=$::log'>Bloat Log</a></small>\n";
}

# Collect all of the log files. Files are assumed to be named
# "refcnt-class-serialno.log", so we'll list all of the files and then
# parse out the 'class' and 'serialno' values to present a pretty
# HTML-ized version.
{
    my @files = $::zip->dir();

    my $current = "";
    my $count = 0;

    my $file;
    FILE: foreach (@files) {
        $_ = $$_{name};
        next FILE unless (/^refcnt-([^-]+)-(\d+).log$/);

	$::classes{$1} = [] if !$::classes{$1};
	my $objects = $::classes{$1};
	push(@$objects, $2);
    }
}

print "<table border='0'>\n";
print "<th><tr bgcolor='#DDDDDD'><td align='center'><b>Class</b></td><td align='center'><b>Objects</b></td></tr></th>\n";
print "<tbody>\n";

{
    my $bgcolor='#FFFFFF';

    my $class;
    foreach $class (sort(keys(%::classes))) {
	print "<tr bgcolor='$bgcolor'>\n";
	print "<td valign='top'><a href='http://lxr.mozilla.org/seamonkey/ident?i=$class'>$class</a></td>\n";
	print "<td><small>\n";
	my $objects = $::classes{$class};

	my $object;
	foreach $object (sort { $::a <=> $::b } @$objects) {
	    print "<a href='balance.cgi?log=$::log&class=$class&object=$object'>$object</a>\n";
	}
	print "\n</small></td>\n";
	print "</tr>\n";

	$bgcolor = ($bgcolor eq '#FFFFFF') ? '#FFFFBB' : '#FFFFFF';
    }
}

print "</tbody></table>\n";

print $::query->end_html;

