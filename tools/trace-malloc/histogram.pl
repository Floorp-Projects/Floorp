#!/usr/bin/perl -w
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is histogram.pl, released
# Nov 27, 2000.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Chris Waterson <waterson@netscape.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

# This program produces a ``class histogram'' of the live objects, one
# line per class, with the total number of objects allocated, and
# total number of bytes attributed to those objects.

use 5.004;
use strict;
use Getopt::Long;

# So we can find TraceMalloc.pm
use FindBin;
use lib "$FindBin::Bin";

use TraceMalloc;

# Collect program options
$::opt_help = 0;
$::opt_types = "${FindBin::Bin}/types.dat";

GetOptions("help", "types=s");

if ($::opt_help) {
    die "usage: histogram.pl [options] <dumpfile>
  --help          Display this message
  --types=<file>  Read type heuristics from <file>";
}

# Initialize type inference juju from the type file specified by
# ``--types''.
if ($::opt_types) {
    TraceMalloc::init_type_inference($::opt_types);
}

# Read the dump file, collecting count and size information for each
# object that's detected.

# This'll hold a record for each class that we detect
$::Classes = { };

sub collect_objects($) {
    my ($object) = @_;
    my $type = $object->{'type'};

    my $entry = $::Classes{$type};
    if (! $entry) {
        $entry = $::Classes{$type} = { '#count#' => 0, '#bytes#' => 0 };
    }

    $entry->{'#count#'} += 1;
    $entry->{'#bytes#'} += $object->{'size'};
}

TraceMalloc::read(\&collect_objects);

# Print one line per class, sorted with the classes that accumulated
# the most bytes first.
foreach my $class (sort { $::Classes{$b}->{'#bytes#'} <=> $::Classes{$a}->{'#bytes#'} } keys %::Classes) {
    print "$class $::Classes{$class}->{'#count#'} $::Classes{$class}->{'#bytes#'}\n";
}
