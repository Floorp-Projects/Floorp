#!/usr/bonsaitools/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
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
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Matthew Tuck <matty@chariot.net.au>

# This script compiles all the documentation.

use diagnostics;
use strict;

use File::Basename;

###############################################################################
# Environment Variable Checking
###############################################################################

my ($JADE_PUB, $LDP_HOME);

if (defined $ENV{JADE_PUB} && $ENV{JADE_PUB} ne '') {
    $JADE_PUB = $ENV{JADE_PUB};
}
else {
    die "You need to set the JADE_PUB environment variable first.";
}

if (defined $ENV{LDP_HOME} && $ENV{LDP_HOME} ne '') {
    $LDP_HOME = $ENV{LDP_HOME};
}
else {
    die "You need to set the LDP_HOME environment variable first.";
}

###############################################################################
# Subs
###############################################################################

sub MakeDocs($$) {

    my ($name, $cmdline) = @_;

    print "Creating $name documentation ...\n";
    print "$cmdline\n\n";
    system $cmdline;
    print "\n";

}

###############################################################################
# Make the docs ...
###############################################################################

chdir dirname($0);
chdir 'html';

MakeDocs('separate HTML', "jade -t sgml -i html -d $LDP_HOME/ldp.dsl\#html " .
	 "$JADE_PUB/xml.dcl ../sgml/Bugzilla-Guide.sgml");
MakeDocs('big HTML', "jade -V nochunks -t sgml -i html -d " .
         "$LDP_HOME/ldp.dsl\#html $JADE_PUB/xml.dcl " .
	 "../sgml/Bugzilla-Guide.sgml > Bugzilla-Guide.html");
MakeDocs('big text', "lynx -dump -justify=off -nolist Bugzilla-Guide.html " .
	 "> ../txt/Bugzilla-Guide.txt");
