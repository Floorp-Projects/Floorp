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
# Contributor(s): Jacob Steenhagen <jake@bugzilla.org>
#                 David D. Kilzer <ddkilzer@kilzer.net>
#

package Support::Templates;

use strict;

use lib 't';
use vars qw($include_path @referenced_files @actual_files);

use Support::Files;

use File::Find;
use File::Spec 0.82;

# Note that $include_path is assumed to only contain ONE path, not
# a list of colon-separated paths.
$include_path = File::Spec->catdir('template', 'en', 'default');
@referenced_files = ();
@actual_files = ();

# Local subroutine used with File::Find
sub find_templates {
    # Prune CVS directories
    if (-d $_ && $_ eq 'CVS') {
        $File::Find::prune = 1;
        return;
    }

    # Only include files ending in '.tmpl'
    if (-f $_ && $_ =~ m/\.tmpl$/i) {
        my $filename;
        my $local_dir = File::Spec->abs2rel($File::Find::dir,
                                            $File::Find::topdir);

        if ($local_dir) {
            $filename = File::Spec->catfile($local_dir, $_);
        } else {
            $filename = $_;
        }

        push(@actual_files, $filename);
    }
}

# Scan the template include path for templates then put them in
# in the @actual_files array to be used by various tests.
map(find(\&find_templates, $_), split(':', $include_path));

# Scan Bugzilla's perl code looking for templates used and put them
# in the @referenced_files array to be used by the 004template.t test.
my %seen;

foreach my $file (@Support::Files::testitems) {
    open (FILE, $file);
    my @lines = <FILE>;
    close (FILE);
    foreach my $line (@lines) {
        if ($line =~ m/template->process\(\"(.+?)\", .+?\)/) {
            my $template = $1;
            # Ignore templates with $ in the name, since they're
            # probably vars, not real files
            next if $template =~ m/\$/;
            next if $seen{$template};
            push (@referenced_files, $template);
            $seen{$template} = 1;
        }
    }
}

1;
