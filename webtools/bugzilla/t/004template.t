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

use strict;
use Template;

my @testitems = @Support::Templates::testitems;
my $include_path = $Support::Templates::include_path;
# Capture the TESTERR from Test::More for printing errors.
# This will handle verbosity for us automatically
*TESTOUT = \*Test::More::TESTOUT;

# Check to make sure all templates that are referenced in
# Bugzilla exist in the proper place.

my %exists;
foreach my $file(@testitems) {
    if (-e $include_path . "/" . $file) {
        ok(1, "$file exists");
        $exists{$file} = 1;
    } else {
        ok(0, "$file does not exist --ERROR");
    }
}

# Processes all the templates to make sure they have good syntax
my $template = Template->new(
{
    INCLUDE_PATH => $include_path ,
    RELATIVE => 1,
    # Need to define filters used in the codebase, they don't
    # actually have to function in this test, just be defined.
    FILTERS =>
    {
      url => sub { return $_ } ,
    } 
}
);

open SAVEOUT, ">&STDOUT";     # stash the original output stream
open SAVEERR, ">&STDERR";
open STDOUT, "> /dev/null";   # discard all output
open STDERR, "> /dev/null";
foreach my $file(@testitems) {
    if ($exists{$file}) {
        if ($template->process($file)) {
            ok(1, "$file syntax ok");
        }
        else {
            print TESTOUT $template->error() . "\n";
            ok(0, "$file has bad syntax --ERROR");
        }
    }
    else {
        ok(1, "$file doesn't exists, skipping test");
    }
}
open STDOUT, ">&SAVEOUT";     # redirect back to original stream
open STDERR, ">&SAVEERR";
close SAVEOUT;
close SAVEERR;
