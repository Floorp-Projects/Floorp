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
# $Id: bloat-log.cgi,v 1.1 1999/11/15 22:31:33 waterson%netscape.com Exp $
#

#
# Extracts the original bloat log from a Leak-o-Matic zip
#

use 5.004;
use strict;
use CGI;
use POSIX;
use Zip;

$::query = new CGI();

# The ZIP where all the log files are kept
$::log = $::query->param('log' => 'current');
$::zip = new Zip($::log);

print $::query->header;

{
    my @statinfo = stat($::log);
    my $when = POSIX::strftime "%a %b %e %H:%M:%S %Y", localtime($statinfo[9]);

    print $::query->start_html("Bloat Log, $when"),
          $::query->h1("Bloat Log");

    print "<small>$when</small>\n";
}

print "<pre>\n";

{
    my $handle = $::zip->expand('master-bloat.log');

    while (<$handle>) {
        print $_;
    }
}

print "</pre>\n";
print $::query->end_html;
