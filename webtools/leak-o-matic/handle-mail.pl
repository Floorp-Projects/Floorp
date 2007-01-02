#!/usr/bin/perl
# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is Mozilla Leak-o-Matic.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corp.  Portions created by Netscape Communucations
# Corp. are Copyright (C) 1999 Netscape Communications Corp.  All
# Rights Reserved.
# 
# Contributor(s):
# Chris Waterson <waterson@netscape.com>
# 
# $Id: handle-mail.pl,v 1.5 2007/01/02 22:54:24 timeless%mozdev.org Exp $
#

#
# ``sendmail'' handler to receive data on the server. Cooperates with
# ``make-data.pl'', the data collection script.
#
# To use, create an account on your server; e.g., "leak-o-matic", that
# will receive mail from the data collector. In this account's home
# directory, create a .forward file that containes the following
#
# "|handle-mail.pl --datadir=whatever"
#
# Yes, the quotes are important. Depending on your sendmail
# configuration, you may need to place the ``handle-mail.pl'' script
# in a special directory; e.g., /etc/smrsh on a vanilla RH6.0 system.
#

use 5.006;
use strict;
use Getopt::Long;
use File::Copy;

GetOptions("datadir=s");
chdir('/tmp');

# Open a pipe to ``uudecode'' so we can unstuff the binary
open(OUT, "|uudecode");

LINE: while (<>) {
    # Ignore anything up until the ``begin'' directive
    if (/^begin \d\d\d (.*)/) {
	$::file = $1;
	print OUT $_;
	last LINE;
    }
}

# No file means we read the entire message and found nothing
$::file || die;

while (<>) {
	print OUT $_;
}

# Be sure to close the pipe (so uudecode can complete) before moving
# the file
close(OUT);

# Move to the data directory, if there is one.
move($::file, $::opt_datadir) if $::opt_datadir;
# $::opt_datadir does not seem to be set in current CVS.
