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
# The Original Code are the Bugzilla tests.
#
# The Initial Developer of the Original Code is Jacob Steenhagen.
# Portions created by Jacob Steenhagen are
# Copyright (C) 2001 Jacob Steenhagen. All
# Rights Reserved.
#
# Contributor(s): Jacob Steenhagen <jake@acutex.net>
#

#################
#Bugzilla Test 5#
#####no_tabs#####

BEGIN { use lib "t/"; }
BEGIN { use Support::Files; }
BEGIN { $tests = @Support::Files::testitems; }
BEGIN { use Test::More tests => $tests; }

use strict;

my @testitems = @Support::Files::testitems;
my $verbose = $::ENV{TEST_VERBOSE};

foreach my $file (@testitems) {
    open (FILE, "$file");
    my @file = <FILE>;
    close (FILE);
    if (grep /\t/, @file) {
        ok(0, "$file contains tabs --WARNING");
    } else {
        ok(1, "$file has no tabs");
    }
}

