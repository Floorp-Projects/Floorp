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
#                 David D. Kilzer <ddkilzer@kilzer.net>
#

#################
#Bugzilla Test 5#
#####no_tabs#####

use diagnostics;
use strict;

use lib 't';

use Support::Files;
use Support::Templates;

use File::Spec 0.82;
use Test::More tests => (  scalar(@Support::Files::testitems)
                         + scalar(@Support::Templates::actual_files));

my @testitems = @Support::Files::testitems;
my @templates = map(File::Spec->catfile($Support::Templates::include_path, $_),
                    @Support::Templates::actual_files);
push(@testitems, @templates);

foreach my $file (@testitems) {
    open (FILE, "$file");
    if (grep /\t/, <FILE>) {
        ok(0, "$file contains tabs --WARNING");
    } else {
        ok(1, "$file has no tabs");
    }
    close (FILE);
}

