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
#Bugzilla Test 4#
##Templates######

BEGIN { use lib "t/"; }
BEGIN { use Support::Templates; }
BEGIN { $tests = @Support::Templates::testitems * 2; }
BEGIN { use Test::More tests => $tests; }

use Template;

my @testitems = @Support::Templates::testitems;
my $include_path = $Support::Templates::include_path;
my $verbose = $::ENV{VERBOSE};

# Check to make sure all templates that are referenced in
# Bugzilla exist in the proper place.

foreach my $file(@testitems) {
    if (-e $include_path . "/" . $file) {
        ok(1, "$file exists");
    } else {
        ok(0, "$file does not exist --ERROR");
    }
}

# Processes all the templates to make sure they have good syntax
my $template = Template->new ({
                 INCLUDE_PATH => $include_path,
                 RELATIVE => 1
                 });

open SAVEOUT, ">&STDOUT";     # stash the original output stream
open STDOUT, "> /dev/null";   # discard all output
foreach my $file(@testitems) {
    if ($template->process($file)) {
        ok(1, "$file syntax ok");
    } else {
        ok(0, "$file has bad syntax --ERROR");
    }
}
open STDOUT, ">&SAVEOUT";     # redirect back to original stream
close SAVEOUT;
