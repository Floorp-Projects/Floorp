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

package Support::Templates;

$include_path = "template/default";

# Scan Bugzilla's code looking for templates used and put them
# in the @testitems array to be used by the template.t test.

my @files = glob('*');
my %t;

foreach my $file (@files) {
    open (FILE, $file);
    my @lines = <FILE>;
    close (FILE);
    foreach my $line (@lines) {
	if ($line =~ m/template->process\(\"(.+?)\", .+?\)/) {
            $template = $1;
	    push (@testitems, $template) unless $t{$template};
	    $t{$template} = 1;
	}
    }
}

# Now let's look at the templates and find any other templates
# that are INCLUDEd.
foreach my $file(@testitems) {
    open (FILE, $include_path . "/" . $file) || next;
    my @lines = <FILE>;
    close (FILE);
    foreach my $line (@lines) {
        if ($line =~ m/\[% INCLUDE (.+?) /) {
            my $template = $1;
            push (@testitems, $template) unless $t{$template};
            $t{$template} = 1;
        }
    }
}

1;
